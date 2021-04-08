#ifndef _PIPELINE_MGR_H_
#define _PIPELINE_MGR_H_

#include <stdint.h>

/**
 * @brief Run computations on camera frames.
 * @param win_debug If the debug window showing the procesed frrame should be shown (1) or not (0).
 * This can of course only be used when @p cam_or_proc is set to 0.
 * @param cam_or_proc Decide which pipeline to run. When 1, a pipeline which reads camera frames and
 * writes them to state shmem will be run. When 0, a frame processing pipeline will run which reads
 * frames from state shmem and processes them.
 * @param proc_func A function which will process a frame and use its data in any way it wants.
 * @param proc_func_args Pointer to arguments which will be passed to proc_fucn when it is called.
 * @param user_deinit Pointer to a function which gets run when daemon exit is requested.
 * @return 0 on success, 1 on failure
 */
int pl_mgr_run(uint8_t const win_debug, uint8_t const cam_or_proc, void (*const proc_func)(uint8_t (*const)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH], int const, void *const), void *const proc_func_args, int (*const user_deinit)(void));

#endif /* _PIPELINE_MGR_H_ */