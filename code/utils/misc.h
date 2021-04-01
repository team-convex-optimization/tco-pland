#ifndef _MISC_H_
#define _MISC_H_

#include <stdint.h>
#include "tco_shmem.h"
#include "tco_linalg.h"

/**
 * @brief General purpose bresenham implementation. It takes in a callback which gets called for
 * every pixel traced by this algorithm.
 * @param pixel_action The user defined callback which gets called for every traced pixel. Should
 * return 0 if tracing should continue and -1 if it should stop.
 * @param start Where the line should start.
 * @param end Where the line should end.
 * @note A customized version of https://en.wikipedia.org/wiki/Bresenham%27s_line_algorithm.
 */
void bresenham(uint8_t (*pixels)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH],
               uint8_t (*pixel_action)(uint8_t (*const)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH], point2_t const),
               point2_t const start,
               point2_t const end);

#endif /* _MISC_H_ */