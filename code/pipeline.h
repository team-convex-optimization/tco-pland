#ifndef _PIPELINE_H_
#define _PIPELINE_H_
/* Abbreviation for 'pipeline' adopted here is 'pl'. */

#include <stdint.h>

typedef struct frame_processor_t
{
    void (*func)(uint8_t (*)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH], int, void *); /* Pixels | Length | Args */
    void *args;
} frame_processor_t;

typedef struct frame_injector_t
{
    void (*func)(uint8_t (*)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH], int, void *); /* Pixel destination | Length | Args */
    void *args;
} frame_injector_t;

typedef struct pl_user_data_t
{
    frame_processor_t frame_processor_data;
    frame_injector_t frame_injector_data;
} pl_user_data_t;

/**
 * @brief Run the 'camera pipeline'.
 * @param user_data Provides a definition for the frame processor to pass the raw frames to.
 * @note The frame injector definition passed in user data to this function will be ignored.
 * @return 0 on success and -1 on failure.
 */
int pl_camera_pipeline_run(pl_user_data_t *const user_data);

/**
 * @brief Run the 'proc pipeline'.
 * @param user_data Provides a definition for the frame processor to pass the frames to and the
 * frame injector which will acquire the frames and 'inject' them into the pipeline..
 * @return 0 on success and -1 on failure.
 */
int pl_proc_pipeline_run(pl_user_data_t *const user_data);

/**
 * @brief Run the 'display pipeline'.
 * @param user_data Provides a definition for the frame injector which should 'inject' processed
 * frame data into the pipeline.
 * @note The frame processor definition passed in user data to this function will be ignored.
 * @return 0 on success and -1 on failure.
 */
int pl_display_pipeline_run(pl_user_data_t *const user_data);

#endif /* _PIPELINE_H_ */