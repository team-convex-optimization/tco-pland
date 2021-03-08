#include <stdlib.h>
#include <stdio.h>
#include <stdatomic.h>

#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <pthread.h>
#include <time.h>

#include "tco_libd.h"
#include "tco_shmem.h"
#include "camera.h"
#include "trajection.h"
#include "preprocessing.h"

int log_level = LOG_INFO | LOG_ERROR | LOG_DEBUG;

static struct tco_shmem_data_training *training_data;
static sem_t *training_data_sem;
static uint8_t shmem_open = 0; /* To ensure that semaphor is never left at 0 when forcing the app to exit. */
static uint32_t const frame_size_expected = TCO_SIM_WIDTH * TCO_SIM_HEIGHT * sizeof(uint8_t);

/* This will be accessed by multiple threads. The alignment is there to avoid problems when using memcpy with this address. */
static uint8_t __attribute__((aligned(32))) frame_processed[TCO_SIM_HEIGHT][TCO_SIM_WIDTH] = {0};
static pthread_mutex_t frame_processed_mutex;

static uint8_t using_threads = 0;
static pthread_t thread_display = {0};
static pthread_t thread_camera_sim = {0};
static atomic_char exit_requested = 0; /* Gets written by all pthreads and gets read in the main thread. */

/**
 * @brief This method is used as a handler for various basic signals. It does not do much but it 
 * ensures that the main thread which polls the 'exit_requested' variable, known that it should 
 * cleanup and exit.
 * @param sig Signal that has occured and which needs to be handled.
 */
static void handle_signals(int sig)
{
  /* TODO: Not all signals should just quit. */
  atomic_store(&exit_requested, 1);
}

/**
 * @brief Inject a diagonally scrolling grayscale gradient to the destination pointer.
 * @param pixel_dest Where the frame will be written.
 * @param length The length of the 'pixel_dest' array in bytes.
 * @param args_ptr Pointer to user data in particular the 'frame_injector_t' args field.
 */
static void frame_test_injector(uint8_t *pixel_dest, int length, void *args_ptr)
{
  static float offset = 0.0;           /* Offset of the gradient along the diagonal as a fraction of the diagonal length. */
  const float pix_val_white = 255.0f;  /* 0=black, 255=white. */
  const float speed_scrolling = 0.01f; /* As a fraction of the diagonal length per frame. */
  float pix_offset;
  for (uint16_t y = 0; y < TCO_SIM_HEIGHT; y++)
  {
    for (uint16_t x = 0; x < TCO_SIM_WIDTH; x++)
    {
      /* Fraction from upper left corner along the diagonal (1 = bottom right corner, 0 = top left corner). */
      pix_offset = ((x / TCO_SIM_WIDTH) + (y / TCO_SIM_HEIGHT)) / 2;
      /* An offset is in the range of [0..1] where at 0 upper left corner is all black and gradient 
      moves diagonally until white in the bottom right. Offset equal to 1 means the upper left 
      corner is white and bottom right corner is black. */
      pix_offset += offset;
      if (pix_offset > 1.0f)
      {
        pix_offset -= 1.0f;
      }
      pixel_dest[x + (y * TCO_SIM_WIDTH)] = (int)(pix_offset * pix_val_white);
    }
  }
  offset += speed_scrolling;
  if (offset > 1.0f)
  {
    offset -= 1.0f;
  }
}

/**
 * @brief Receives pixels and does processing on them. This is the function which a user should 
 * modify to perform processing on a frame.
 * @param pixels The pointer to the raw grayscale frame received from the video pipeline. This is 
 * guaranteed to be a 2D array whose size is TCO_SIM_WIDTH by TCO_SIM_HEIGHT.
 * @param length The size of the pixels array in bytes.
 * @param args_ptr Pointer to user data in particular the 'frame_processor_t' args field.
 */
static void frame_raw_processor(uint8_t *pixels, int length, void *args_ptr)
{
  if (frame_size_expected != length)
  {
    log_error("The expected frame size and actual frame size do not match");
    atomic_store(&exit_requested, 1);
    exit(EXIT_FAILURE);
  }
  /* This is an array which holds a copy of the processed frame. This is done because nothing can 
  be guaranteed about the 'pixels' pointer (it could even be read-only). Needs to be aligned in 
  order for memcpy to be used. */
  uint8_t __attribute__((aligned(32))) frame_processed_tmp[TCO_SIM_HEIGHT][TCO_SIM_WIDTH] = {0};
  memcpy(&frame_processed_tmp, pixels, frame_size_expected);

  /* Process image here by modifying 'frame_processed_tmp'. */
  convert_threshold((uint8_t *)&frame_processed_tmp);
  k_nearest_neighbor_gradient_center((uint8_t *)&frame_processed_tmp, 3);

  if (pthread_mutex_lock(&frame_processed_mutex) != 0)
  {
    log_error("pthread_mutex_lock: %s", strerror(errno));
    log_error("Failed to lock mutex for accessing processed frame data inside 'frame_raw_processor'");
    atomic_store(&exit_requested, 1);
    exit(EXIT_FAILURE);
  }
  memcpy(&frame_processed, &frame_processed_tmp, frame_size_expected);
  if (pthread_mutex_unlock(&frame_processed_mutex) != 0)
  {
    log_error("pthread_mutex_unlock: %s", strerror(errno));
    log_error("Failed to unlock mutex for accessing processed frame data inside 'frame_raw_processor'");
    atomic_store(&exit_requested, 1);
    exit(EXIT_FAILURE);
  }
}

/**
 * @brief Reads a frame from the simulator written shared memory and writes them to the pixel 
 * destination pointer.
 * @param pixel_dest The location where the frame will be written.
 * @param length The size of the pixels array in bytes.
 * @param args_ptr Pointer to user data in particular the 'frame_injector_t' args field.
 */
static void frame_raw_injector(uint8_t *pixel_dest, int length, void *args_ptr)
{
  if (frame_size_expected != length)
  {
    log_error("The expected frame size and actual frame size do not match");
    atomic_store(&exit_requested, 1);
    exit(EXIT_FAILURE);
  }

  if (sem_wait(training_data_sem) == -1)
  {
    log_error("sem_wait: %s", strerror(errno));
    atomic_store(&exit_requested, 1);
    exit(EXIT_FAILURE);
  }
  shmem_open = 1;
  memcpy(pixel_dest, &(training_data->video), frame_size_expected);
  if (sem_post(training_data_sem) == -1)
  {
    log_error("sem_post: %s", strerror(errno));
    atomic_store(&exit_requested, 1);
    exit(EXIT_FAILURE);
  }
  shmem_open = 0;
}

/**
 * @brief Reads the processed simulator frame data and writes it to the pixel destination pointer.
 * @param pixel_dest The location where the frame will be written.
 * @param length The size of the pixels array in bytes.
 * @param args_ptr Pointer to user data in particular the 'frame_injector_t' args field.
 */
static void frame_processed_injector(uint8_t *pixel_dest, int length, void *args_ptr)
{
  if (frame_size_expected != length)
  {
    log_error("The expected frame size and actual frame size do not match");
    atomic_store(&exit_requested, 1);
    exit(EXIT_FAILURE);
  }

  /* Reading from the 'frame_processed' array is meant to be done by any thread hence to allow 
  inconsistencies, in order to read the contents, a mutex must be locked. */
  if (pthread_mutex_lock(&frame_processed_mutex) != 0)
  {
    log_error("pthread_mutex_lock: %s", strerror(errno));
    log_error("Failed to lock mutex for accessing processed frame data inside 'frame_processed_injector'");
    atomic_store(&exit_requested, 1);
    exit(EXIT_FAILURE);
  }
  memcpy(pixel_dest, &frame_processed, frame_size_expected);
  if (pthread_mutex_unlock(&frame_processed_mutex) != 0)
  {
    log_error("pthread_mutex_unlock: %s", strerror(errno));
    log_error("Failed to unlock mutex for accessing processed frame data inside 'frame_processed_injector'");
    atomic_store(&exit_requested, 1);
    exit(EXIT_FAILURE);
  }
}

/**
 * @brief A function which is meant to be run by a child thread and run the display pipeline i.e. 
 * the pipeline which reads the processsed simulator frame and displays it on screen in a window.
 * @param arg Pointer to user data passed to this job.
 */
static void *thread_job_display_pipeline(void *args)
{
  camera_user_data_t user_data_display = {{NULL, NULL}, {&frame_processed_injector, NULL}};
  if (display_pipeline_run(&user_data_display) != 0)
  {
    log_error("Failed to run the display pipeline");
  }
  log_info("Display thread is quitting");
  atomic_store(&exit_requested, 1);
  return NULL;
}

/**
 * @brief A function which is meant to be run by a child thread and run the simulator camera 
 * pipeline i.e. the pipeline which reads raw frames from the shared memory where the simulator 
 * writes its frame, then passes this data to a user controlled processing function.
 * @param arg Pointer to user data passed to this job.
 */
static void *thread_job_camera_sim_pipeline(void *args)
{
  camera_user_data_t user_data_camera_sim = {{&frame_raw_processor, NULL}, {&frame_raw_injector, NULL}};
  if (camera_sim_pipeline_run(&user_data_camera_sim) != 0)
  {
    log_error("Failed to run the simulator camera pipeline");
  }
  log_info("Camera sim thread is quitting");
  atomic_store(&exit_requested, 1);
  return NULL;
}

/**
 * @brief A helped function which registers the signal handler for all common signals.
 */
static void register_signal_handler(void)
{
  struct sigaction sa;
  sa.sa_handler = handle_signals;
  sigfillset(&sa.sa_mask);
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGHUP, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);
}

/**
 * @brief It handles the situation where any of the threads request that the application exits. 
 * Before exitting, this function will ensure that all necessary cleanup is done e.g. posting the 
 * shmem access semaphore to avoid deadlocking the other processes which depend on the semaphore 
 * to work as expected.
 * @note This function should be called once all children threads are created but where the main 
 * thead is not joined to any of the children threads.
 */
static void detect_and_handle_exit_requested(void)
{
  /* 100ms but not exactly due to granularity of the clock hence the 'rem' pointer passed to 
  'nanosleep' */
  struct timespec const req = {0, 100000000};
  struct timespec rem;

  while (!atomic_load(&exit_requested))
  {
    /* Wait until termination is requested. */
    nanosleep(&req, &rem);
  }

  log_info("Cleaning up before termination");
  if (shmem_open)
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
}

int main(int argc, char *argv[])
{
  if (log_init("pland", "./log.txt") != 0)
  {
    printf("Failed to initialize the logger\n");
    return EXIT_FAILURE;
  }

  if (argc == 2 && (strcmp(argv[1], "--sandbox") == 0 || strcmp(argv[1], "-s") == 0))
  {
    /* Run the pipeline which reads simulated camera data from 'tco_sim' and processes it using 
    the function as it would when running the real camera pipeline. 

    Since it is useful to see what is going on when testing, a second pipeline must also be 
    running which displays the processed franes. To do achieve this, 2 new children threads are 
    created, one running the pipeline which reads camera data and passes it onto the processing 
    function, and another which reads the output of the processing function and displays the 
    resulting frame in a window. */
    using_threads = 1;
    register_signal_handler();

    atomic_init(&exit_requested, 0);

    shmem_open = 1;
    /* This could also just be done inside the thread which uses it but would make cleanup more 
    difficult. */
    if (shmem_map(TCO_SHMEM_NAME_TRAINING,
                  TCO_SHMEM_SIZE_TRAINING,
                  TCO_SHMEM_NAME_SEM_TRAINING,
                  O_RDONLY,
                  (void **)&training_data,
                  &training_data_sem) != 0)
    {
      log_error("Failed to map shared memory and associated semaphore");
      return EXIT_FAILURE;
    }

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

    detect_and_handle_exit_requested();
  }
  else
  {
    /* Run the real camera pipeline which reads raw frames and passes them to a user controlled 
    processing function.

    Since running a single pipeline does not require multiple children threads, it is ran from 
    the main thread. */
    camera_user_data_t user_data = {{NULL, NULL}, {NULL, NULL}};
    if (camera_pipeline_run(&user_data) != 0)
    {
      log_error("Failed to run the camera pipeline");
      return EXIT_FAILURE;
    }
  }

  return EXIT_SUCCESS;
}
