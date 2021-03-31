#ifndef _CAM_MGR_H_
#define _CAM_MGR_H_

#include <stdint.h>

/**
 * @brief Run computations on camera frames.
 * @param real_or_sim 0 means the computations will run on the output of a real camera and 1 means
 * they will be done on the output of a simulated camera (and additionally a debug window will be
 * shown with the computation output).
 * @param proc_func A function which will process a frame and use its data in any way it wants.
 * @param proc_func_args Pointer to arguments which will be passed to proc_fucn when it is called.
 * @param user_deinit Pointer to a function which gets run when daemon exit is requested.
 * @return 0 on success, 1 on failure
 */
int cam_mgr_run(uint8_t real_or_sim, void (*proc_func)(uint8_t (*)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH], int, void *), void *proc_func_args, int (*user_deinit)(void));

#endif /* _CAM_MGR_H_ */