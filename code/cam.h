#ifndef _CAM_H_
#define _CAM_H_

#include <stdint.h>

typedef struct frame_processor_t
{
    void (*func)(uint8_t *, int, void *); /* Pixels | Length | Args */
    void *args;
} frame_processor_t;

typedef struct frame_injector_t
{
    void (*func)(uint8_t *, int, void *); /* Pixel destination | Length | Args */
    void *args;
} frame_injector_t;

typedef struct cam_user_data_t
{
    frame_processor_t frame_processor_data;
    frame_injector_t frame_injector_data;
} cam_user_data_t;

/**
 * @brief Run the 'camera pipeline' which takes frames from the real camera and passes them onto the
 * user-defined processing functions defined in user data.
 * @param user_data Provides a definition for the frame processor to pass the raw frames to.
 * @note The frame injector definition passed in user data to this function will be ignored.
 * @return 0 on success and -1 on failure.
 */
int cam_pipeline_run(cam_user_data_t *const user_data);

/**
 * @brief Run the 'simulator camera pipeline' which takes raw frames from the simulator (by reading
 * shared memory) and passes them onto the user-defined processing function defined in the user
 * data.
 * @param user_data Provides a definition for the frame processor to pass the frames to and the
 * frame injector which will acquire the frames and 'inject' them into the pipeline..
 * @return 0 on success and -1 on failure.
 */
int cam_sim_pipeline_run(cam_user_data_t *const user_data);

/**
 * @brief Run the 'display pipeline' which takes processed frames which were output from the user
 * defined processing function, and displays them in a window.
 * @param user_data Provides a definition for the frame injector which should 'inject' processed
 * frame data into the pipeline.
 * @note The frame processor definition passed in user data to this function will be ignored.
 * @return 0 on success and -1 on failure.
 */
int cam_display_pipeline_run(cam_user_data_t *const user_data);

#endif /* _CAM_MGR_H_ */