#ifndef _PLANNER_H_
#define _PLANNER_H_

#include <stdint.h>

/**
 * @brief Initialize the planner module.
 * @return 0 on success, 1 on failure.
 */
int plnr_init();

/**
 * @brief Runs the planner for a given frame. Planner expected to be called on every frame.
 * @param pixels The frame.
 * @return 0 on success, 1 on failure.
 */
int plnr_step(uint8_t (*const pixels)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH]);

/**
 * @brief Deinitializes the planner module.
 * @return 0 on success, 1 on failure.
 */
int plnr_deinit();

#endif /* _PLANNER_H_ */