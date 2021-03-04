#include <stdlib.h>
#include <stdio.h>

#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <pthread.h>

#include "tco_libd.h"
#include "tco_shmem.h"
#include "camera.h"

int log_level = LOG_INFO | LOG_ERROR;

static struct tco_shmem_data_training *training_data;
static sem_t *training_data_sem;
static uint8_t shmem_open = 0; /* To ensure that semaphor is never left at 0 when forcing the app to exit. */
static uint32_t frame_size_expected = TCO_SIM_WIDTH * TCO_SIM_HEIGHT * sizeof(uint8_t);

static pthread_mutex_t frame_processed_mutex;
static uint8_t frame_processed[TCO_SIM_WIDTH][TCO_SIM_HEIGHT] = {0}; /* This will be accessed by multiple threads. */

static uint8_t using_threads = 0;
static pthread_t thread_display = {0};
static pthread_t thread_camera_sim = {0};

/* Handle ctrl-c */
static void handle_sigint(int sig)
{
  if (shmem_open > 0)
  {
    if (sem_post(training_data_sem) == -1)
    {
      log_error("sem_post: %s", strerror(errno));
      log_error("Failed to close semaphore used to control access to training shmem");
    }
  }
  if (using_threads)
  {
    if (pthread_cancel(thread_display) != 0)
    {
      log_error("Failed to cancel display thread");
    }
    if (pthread_cancel(thread_camera_sim) != 0)
    {
      log_error("Failed to cancel camera sim thread");
    }
    if (pthread_mutex_destroy(&frame_processed_mutex) != 0)
    {
      log_error("Failed to destroy mutex for accessing processed frame data");
    }
  }
  exit(EXIT_SUCCESS);
}

/* Inject a diagonally scrolling grayscale gradient. */
static void frame_test_injector(uint8_t *pixel_dest, int length, void *args_ptr)
{
  static float offset = 0.0;
  float pix_offset;
  for (uint16_t y = 0; y < 480; y++)
  {
    for (uint16_t x = 0; x < 640; x++)
    {
      pix_offset = (((x / 640.0) + (y / 480.0)) / 2) + offset;
      if (pix_offset > 1.0f)
      {
        pix_offset -= 1.0f;
      }
      pixel_dest[x + (y * 640)] = (int)(pix_offset * 255.0);
    }
  }
  offset += 0.01;
  if (offset > 1.0f)
  {
    offset -= 1.0f;
  }
}

static void frame_raw_processor(uint8_t *pixels, int length, void *args_ptr)
{
  log_debug("Received pixels. Length: %i", length);
  if (frame_size_expected != length)
  {
    log_error("The expected frame size and actual frame size do not match");
    exit(EXIT_FAILURE);
  }
  uint8_t frame_processed_tmp[TCO_SIM_WIDTH][TCO_SIM_HEIGHT] = {0};
  memcpy(&frame_processed_tmp, &pixels, frame_size_expected);

  /* Process image here by modifying "frame_processed_tmp". */

  if (pthread_mutex_lock(&frame_processed_mutex) != 0)
  {
    log_error("Failed to lock mutex for accessing processed frame data inside 'frame_raw_processor'");
    exit(EXIT_FAILURE);
  }
  memcpy(&frame_processed_tmp, &frame_processed, frame_size_expected);
  if (pthread_mutex_unlock(&frame_processed_mutex) != 0)
  {
    log_error("Failed to unlock mutex for accessing processed frame data inside 'frame_raw_processor'");
    exit(EXIT_FAILURE);
  }
}

static void frame_raw_injector(uint8_t *pixel_dest, int length, void *args_ptr)
{
  if (frame_size_expected != length)
  {
    log_error("The expected frame size and actual frame size do not match");
    exit(EXIT_FAILURE);
  }
  if (sem_wait(training_data_sem) == -1)
  {
    log_error("sem_wait: %s", strerror(errno));
    exit(EXIT_FAILURE);
  }
  /* START: Critical section */
  shmem_open = 1;
  memcpy(pixel_dest, &(training_data->video), frame_size_expected);
  /* END: Critical section */
  if (sem_post(training_data_sem) == -1)
  {
    log_error("sem_post: %s", strerror(errno));
    exit(EXIT_FAILURE);
  }
  shmem_open = 0;
}

static void frame_processed_injector(uint8_t *pixel_dest, int length, void *args_ptr)
{
  if (frame_size_expected != length)
  {
    log_error("The expected frame size and actual frame size do not match");
    exit(EXIT_FAILURE);
  }
  if (pthread_mutex_lock(&frame_processed_mutex) != 0)
  {
    log_error("Failed to lock mutex for accessing processed frame data inside 'frame_processed_injector'");
    exit(EXIT_FAILURE);
  }
  memcpy(&frame_processed, pixel_dest, frame_size_expected);
  if (pthread_mutex_unlock(&frame_processed_mutex) != 0)
  {
    log_error("Failed to unlock mutex for accessing processed frame data inside 'frame_processed_injector'");
    exit(EXIT_FAILURE);
  }
}

static void *thread_job_display_pipeline(void *arg)
{
  camera_user_data_t user_data_display = {{NULL, NULL}, {&frame_processed_injector, NULL}};
  if (display_pipeline_run(&user_data_display) != 0)
  {
    log_error("Failed to run the display pipeline");
    exit(EXIT_FAILURE);
  }
  return NULL;
}

static void *thread_job_camera_sim_pipeline(void *arg)
{
  /* Mapping shmem inside here gives much better performance than mapping it in the main thread. */
  if (shmem_map(TCO_SHMEM_NAME_TRAINING, TCO_SHMEM_SIZE_TRAINING, TCO_SHMEM_NAME_SEM_TRAINING, O_RDONLY, (void **)&training_data, &training_data_sem) != 0)
  {
    log_error("Failed to map shared memory and associated semaphore");
    return NULL;
  }
  camera_user_data_t user_data_camera_sim = {{&frame_raw_processor, NULL}, {&frame_raw_injector, NULL}};
  if (camera_sim_pipeline_run(&user_data_camera_sim) != 0)
  {
    log_error("Failed to run the simulator camera pipeline");
    exit(EXIT_FAILURE);
  }
  return NULL;
}

int main(int argc, char *argv[])
{
  signal(SIGINT, handle_sigint);
  if (log_init("pland", "./log.txt") != 0)
  {
    printf("Failed to initialize the logger\n");
    return EXIT_FAILURE;
  }
  if (argc == 2 && (strcmp(argv[1], "--sandbox") == 0 || strcmp(argv[1], "-s") == 0))
  {
    using_threads = 1;
    if (pthread_mutex_init(&frame_processed_mutex, NULL) != 0)
    {
      log_error("Failed to init a mutex for accessing processed frame data");
      return EXIT_FAILURE;
    }
    if (pthread_create(&thread_display, NULL, &thread_job_display_pipeline, NULL) != 0)
    {
      log_error("Failed to create a thread for displaying processed frame");
      return EXIT_FAILURE;
    }
    if (pthread_create(&thread_camera_sim, NULL, &thread_job_camera_sim_pipeline, NULL) != 0)
    {
      log_error("Failed to create a thread for reading and processing frames from the simulator");
      return EXIT_FAILURE;
    }
    if (pthread_join(thread_display, NULL) != 0)
    {
      log_error("Failed to join display thread");
      return EXIT_FAILURE;
    }
    if (pthread_join(thread_camera_sim, NULL) != 0)
    {
      log_error("Failed to join camera sim thread");
      return EXIT_FAILURE;
    }
  }
  else
  {
    camera_user_data_t user_data = {{NULL, NULL}, {NULL, NULL}};
    if (camera_pipeline_run(&user_data) != 0)
    {
      log_error("Failed to run the camera pipeline");
      return EXIT_FAILURE;
    }
  }
  return EXIT_SUCCESS;
}
