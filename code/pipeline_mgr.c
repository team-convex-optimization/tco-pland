#include <stdlib.h>
#include <stdio.h>
#include <stdatomic.h>

#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <pthread.h>
#include <time.h>

#include "tco_shmem.h"
#include "tco_libd.h"
#include "pipeline.h"
#include "pipeline_mgr.h"
#include "draw.h"

/* A user defined function which receives pointer to frame data and does anything it wants with it.
*/
typedef struct cam_mgr_user_data_t
{
    void (*f)(uint8_t (*)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH], int, void *);
    void *args;
    struct timespec frame_end_times[2]; /* Hold times measured at end of a frame. Used when measuring FPS. */
} cam_mgr_user_data_t;

/* Shared memory state */
static struct tco_shmem_data_state *data_state;
static sem_t *data_state_sem;
static uint8_t shmem_state_open = 0; /* To ensure that semaphor is never left at 0 when forcing the app to exit. */
static uint32_t frame_id_last = 0;
static uint32_t const frame_size_expected = TCO_FRAME_WIDTH * TCO_FRAME_HEIGHT * sizeof(uint8_t);

/* This will be accessed by multiple threads. The alignment is there to avoid problems when using
memcpy with this address. */
static uint8_t _Alignas(4) frame_processed[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH] = {{0}};
static pthread_mutex_t frame_processed_mutex;

/* Thread control state */
static pthread_t thread_display = {0};        /* Thread which runs the display pipeline. */
static pthread_t thread_proc = {0};           /* Thread which runs the processing pipeline.  */
static pthread_t thread_camera = {0};         /* Thread which runs the camera pipeline. */
static atomic_char exit_requested = 0;        /* Gets written by all children threads and gets read in the main thread. */
static cam_mgr_user_data_t compute_user_data; /* While no function should access this variable directly, a reference to it is passed to the frame injecting and processing functions. */

/**
 * @brief This method is used as a handler for various basic signals. It does not do much but it
 * ensures that the main thread which polls the 'exit_requested' variable, known that it should
 * cleanup and exit.
 * @param sig Signal that has occured and which needs to be handled.
 */
static void handle_signals(int sig)
{
    /* XXX: Not all signals should just quit but right now no non-terminating signals are handled by
    this func. */
    atomic_store(&exit_requested, 1);
}

/**
 * @brief A helper function which registers the signal handler for all common signals.
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
 * @brief Perform cleanup such that deadlocks never happen.
 * @param user_deinit A user defined function which deinitializes userspace.
 */
static void cleanup(int (*user_deinit)(void))
{
    log_info("Cleaning up before termination");
    if (shmem_state_open)
    {
        if (sem_post(data_state_sem) == -1)
        {
            log_error("sem_post: %s", strerror(errno));
            log_error("Failed to close semaphore used to control access to state shmem");
        }
    }
    if (pthread_cancel(thread_display) != 0)
    {
        log_error("Failed to cancel display thread");
    }
    if (pthread_cancel(thread_proc) != 0)
    {
        log_error("Failed to cancel proc thread");
    }
    if (pthread_cancel(thread_camera) != 0)
    {
        log_error("Failed to cancel camera thread");
    }
    if (pthread_mutex_destroy(&frame_processed_mutex) != 0)
    {
        log_error("Failed to destroy mutex for accessing processed frame data");
    }
    if (user_deinit != NULL && user_deinit() != 0)
    {
        log_error("User defined deinit function failed");
    }
}

/**
 * @brief It handles the situation where any of the threads request that the application exits.
 * Before exitting, this function will ensure that all necessary cleanup is done e.g. posting the
 * shmem access semaphore to avoid deadlocking the other processes which depend on the semaphore to
 * work as expected.
 * @note This function should be called once all children threads are created but where the main
 * thead is not joined to any of the children threads.
 * @param user_deinit User defined deinit function that will be run before program termination.
 */
static void detect_and_handle_exit_requested(int (*user_deinit)(void))
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
    cleanup(user_deinit);
}

/**
 * @brief Inject a diagonally scrolling grayscale gradient to the destination pointer.
 * @param pixel_dest Where the frame will be written.
 * @param length The length of the 'pixel_dest' array in bytes.
 * @param args_ptr Pointer to user data in particular the 'frame_injector_t' args field.
 * @note A reason for the "unused" attrib is to suppress clang thinking this func is unused
 * unintentionally. This function can be used for testing but is not permanently tied into any
 * pipeline hence is never called hence unused but should stay here in case one wants to check if
 * they are sane ;).
 */
__attribute__((unused)) static void frame_test_injector(uint8_t (*pixel_dest)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH], int length, void *args_ptr)
{
    static float offset = 0.0;           /* Offset of the gradient along the diagonal as a fraction of the diagonal length. */
    const float pix_val_white = 255.0f;  /* 0=black, 255=white. */
    const float speed_scrolling = 0.01f; /* As a fraction of the diagonal length per frame. */
    float pix_offset;
    for (uint16_t y = 0; y < TCO_FRAME_HEIGHT; y++)
    {
        for (uint16_t x = 0; x < TCO_FRAME_WIDTH; x++)
        {
            /* Fraction from upper left corner along the diagonal (1 = bottom right corner, 0 = top
            left corner). */
            pix_offset = ((x / (float)TCO_FRAME_WIDTH) + (y / (float)TCO_FRAME_HEIGHT)) / 2.0f;
            /* An offset is in the range of [0..1] where at 0 upper left corner is all black and
            gradient moves diagonally until white in the bottom right. Offset equal to 1 means the
            upper left corner is white and bottom right corner is black. */
            pix_offset += offset;
            if (pix_offset > 1.0f)
            {
                pix_offset -= 1.0f;
            }
            (*pixel_dest)[y][x] = (uint8_t)(pix_offset * pix_val_white);
        }
    }
    offset += speed_scrolling;
    if (offset > 1.0f)
    {
        offset -= 1.0f;
    }
}

/**
 * @brief Receives pixels and does processing on them.
 * @param pixels The pointer to the raw grayscale frame received from the video pipeline. It is also
 * guaranteed that this array can only be read (not written).
 * @param length The size of the pixels array in bytes.
 * @param args_ptr Pointer to user data in particular the 'frame_processor_t' args field.
 */
static void frame_raw_processor(uint8_t (*pixels)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH], int length, void *args_ptr)
{
    static uint16_t fps_now = 0;
    static uint16_t fps_counter = 0; /* Number of frames that passed in the current second. */

    if (frame_size_expected != length)
    {
        log_error("The expected frame size and actual frame size do not match");
        atomic_store(&exit_requested, 1);
        exit(EXIT_FAILURE);
    }
    /* This is an array which holds a copy of the processed frame since the original is read-only.
    Needs to be aligned in order for memcpy to be used. */
    uint8_t _Alignas(4) frame_processed_tmp[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH] = {{0}};
    memcpy(&frame_processed_tmp, pixels, frame_size_expected);

    draw_q_number(fps_now, (point2_t){10, TCO_FRAME_HEIGHT - 50}, 4);

    /* Process image here by modifying 'frame_processed_tmp'. */
    cam_mgr_user_data_t *compute_user_data = args_ptr;
    compute_user_data->f(&frame_processed_tmp, frame_size_expected, compute_user_data->args);

    /* Measure FPS. */
    if (fps_counter == 0)
    {
        clock_gettime(CLOCK_REALTIME, &compute_user_data->frame_end_times[0]);
    }
    else
    {
        clock_gettime(CLOCK_REALTIME, &compute_user_data->frame_end_times[1]);
    }
    /* Delta is calculated assuming the seconds fields are 0 which they should be in 1 frame. */
    uint64_t const nanos_in_sec = 1000000000;
    struct timespec const delta_time = {0, compute_user_data->frame_end_times[1].tv_nsec - compute_user_data->frame_end_times[0].tv_nsec};
    if (fps_counter > 0 && delta_time.tv_nsec >= nanos_in_sec)
    {
        fps_now = fps_counter;
        memset(&compute_user_data->frame_end_times[0], 0, sizeof(struct timespec));
        memset(&compute_user_data->frame_end_times[1], 0, sizeof(struct timespec));
        fps_counter = 0;
        log_info("fps: %u", fps_now);
    }
    else
    {
        fps_counter += 1;
    }

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
 * @brief Reads a frame from the shared memory and writes them to the pixel destination pointer.
 * @param pixel_dest The location where the frame will be written.
 * @param length The size of the pixels array in bytes.
 * @param args_ptr Pointer to user data in particular the 'frame_injector_t' args field.
 */
static void frame_raw_injector(uint8_t (*pixel_dest)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH], int length, void *args_ptr)
{
    if (frame_size_expected != length)
    {
        log_error("The expected frame size and actual frame size do not match");
        atomic_store(&exit_requested, 1);
        exit(EXIT_FAILURE);
    }

    while (1)
    {
        /* The '!=' ensures that when id wraps around, this will still work. */
        if (data_state->frame_id != frame_id_last)
        {
            if (sem_wait(data_state_sem) == -1)
            {
                log_error("sem_wait: %s", strerror(errno));
                atomic_store(&exit_requested, 1);
                exit(EXIT_FAILURE);
            }
            shmem_state_open = 1;
            memcpy(pixel_dest, &(data_state->frame), frame_size_expected);
            frame_id_last = data_state->frame_id;
            if (sem_post(data_state_sem) == -1)
            {
                log_error("sem_post: %s", strerror(errno));
                atomic_store(&exit_requested, 1);
                exit(EXIT_FAILURE);
            }
            shmem_state_open = 0;
            break;
        }

        /* 20ms but not exactly due to granularity of the clock hence the 'rem' pointer passed to
        'nanosleep' */
        struct timespec const req = {0, 20000000};
        struct timespec rem;
        nanosleep(&req, &rem);
    }
}

/**
 * @brief Reads the processed frame data and writes it to the pixel destination pointer.
 * @param pixel_dest The location where the frame will be written.
 * @param length The size of the pixels array in bytes.
 * @param args_ptr Pointer to user data in particular the 'frame_injector_t' args field.
 */
static void frame_processed_injector(uint8_t (*pixel_dest)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH], int length, void *args_ptr)
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
 * @brief Receives camera frame and saves it in shmem.
 * @param pixels The pointer to the raw grayscale frame received from the camera. It is also
 * guaranteed that this array can only be read (not written).
 * @param length The size of the pixels array in bytes.
 * @param args_ptr This is ignored.
 */
static void frame_cam_processor(uint8_t (*pixels)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH], int length, void *args_ptr)
{
    if (length != frame_size_expected)
    {
        atomic_store(&exit_requested, 1);
        exit(EXIT_FAILURE);
    }

    if (sem_wait(data_state_sem) == -1)
    {
        log_error("sem_wait: %s", strerror(errno));
        atomic_store(&exit_requested, 1);
        exit(EXIT_FAILURE);
    }
    shmem_state_open = 1;
    memcpy(&data_state->frame, pixels, frame_size_expected);
    data_state->frame_id++;
    if (sem_post(data_state_sem) == -1)
    {
        log_error("sem_post: %s", strerror(errno));
        atomic_store(&exit_requested, 1);
        exit(EXIT_FAILURE);
    }
    shmem_state_open = 0;
}

/**
 * @brief A function which is meant to be run by a child thread and run the display pipeline.
 * @param arg Pointer to user data passed to this job.
 */
static void *thread_job_display_pipeline(void *args)
{
    pl_user_data_t user_data_display = {{NULL, NULL}, {&frame_processed_injector, NULL}};
    if (pl_display_pipeline_run(&user_data_display) != 0)
    {
        log_error("Failed to run the display pipeline");
    }
    log_info("Display thread is quitting");
    atomic_store(&exit_requested, 1);
    return NULL;
}

/**
 * @brief A function which is meant to be run by a child thread to run the proc pipeline.
 * @param arg Pointer to user data passed to this job.
 */
static void *thread_job_proc_pipeline(void *args)
{
    cam_mgr_user_data_t *compute_user_data = args; /* Show what the arg pointer is explicitly. */
    pl_user_data_t user_data_proc = {{&frame_raw_processor, compute_user_data}, {&frame_raw_injector, NULL}};
    if (pl_proc_pipeline_run(&user_data_proc) != 0)
    {
        log_error("Failed to run the proc pipeline");
    }
    log_info("Proc thread is quitting");
    atomic_store(&exit_requested, 1);
    return NULL;
}

/**
 * @brief A function which is meant to be run by a child thread and run the camera pipeline.
 * @param arg Pointer to user data passed to this job.
 */
static void *thread_job_camera_pipeline(void *args)
{
    pl_user_data_t user_data_camera = {{&frame_cam_processor, NULL}, {NULL, NULL}};
    if (pl_camera_pipeline_run(&user_data_camera) != 0)
    {
        log_error("Failed to run the camera pipeline");
    }
    log_info("Camera thread is quitting");
    atomic_store(&exit_requested, 1);
    return NULL;
}

/**
 * @brief Run the camera pipeline.
 * @return 0 on success and 1 on failure
 */
static int run_pl_camera()
{
    register_signal_handler();

    atomic_init(&exit_requested, 0);

    if (shmem_map(TCO_SHMEM_NAME_STATE,
                  TCO_SHMEM_SIZE_STATE,
                  TCO_SHMEM_NAME_SEM_STATE,
                  O_RDWR,
                  (void **)&data_state,
                  &data_state_sem) != 0)
    {
        log_error("Failed to map shared memory and associated semaphore");
        return EXIT_FAILURE;
    }

    if (pthread_create(&thread_camera, NULL, &thread_job_camera_pipeline, NULL) != 0)
    {
        log_error("Failed to create a thread for writing camera frames to shmem");
        return EXIT_FAILURE;
    }
    detect_and_handle_exit_requested(NULL);
    return EXIT_SUCCESS;
}

/**
 * @brief Run the proc pipeline.
 * @note When @p win_debug is 1, a second pipeline will be started which displays the processed
 * frames. 
 * @param proc_func A function which will process a frame and use its data in any way it wants.
 * @param proc_func_args Pointer to arguments which will be passed to proc_fucn when it is called.
 * @param user_deinit User defined deinit function that will be run before closing.
 * @param win_debug If a debug window showing the processed frames should be shown.
 * @return 0 on success and 1 on failure
 */
static int run_pl_proc(void (*const proc_func)(uint8_t (*const)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH], int const, void *const), void *const proc_func_args, int (*const user_deinit)(void), uint8_t const win_debug)
{
    register_signal_handler();

    atomic_init(&exit_requested, 0);

    /* This could also just be done inside the thread which uses it but would make cleanup more
    difficult. */
    if (shmem_map(TCO_SHMEM_NAME_STATE,
                  TCO_SHMEM_SIZE_STATE,
                  TCO_SHMEM_NAME_SEM_STATE,
                  O_RDONLY,
                  (void **)&data_state,
                  &data_state_sem) != 0)
    {
        log_error("Failed to map shared memory and associated semaphore");
        return EXIT_FAILURE;
    }

    if (pthread_mutex_init(&frame_processed_mutex, NULL) != 0)
    {
        log_error("Failed to init a mutex for accessing processed frame data");
        return EXIT_FAILURE;
    }

    if (win_debug)
    {
        if (pthread_create(&thread_display, NULL, &thread_job_display_pipeline, NULL) != 0)
        {
            log_error("Failed to create a thread for displaying processed frame");
            return EXIT_FAILURE;
        }
    }

    compute_user_data.f = proc_func;
    compute_user_data.args = proc_func_args;
    if (pthread_create(&thread_proc, NULL, &thread_job_proc_pipeline, &compute_user_data) != 0)
    {
        log_error("Failed to create a thread for reading and processing frames from the simulator");
        return EXIT_FAILURE;
    }

    detect_and_handle_exit_requested(user_deinit);
    return EXIT_SUCCESS;
}

int pl_mgr_run(uint8_t const win_debug, uint8_t const cam_or_proc, void (*const proc_func)(uint8_t (*const)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH], int const, void *const), void *const proc_func_args, int (*const user_deinit)(void))
{
    if (cam_or_proc)
    {
        return run_pl_camera();
    }
    else
    {
        return run_pl_proc(proc_func, proc_func_args, user_deinit, win_debug);
    }
}
