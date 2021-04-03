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

/**
 * @brief Perform a fast but rough radial sweep contour trace. It will trace at most @p
 * contour_length pixels and will travel in @p cw_or_ccw (clockwise or counter-clockwise) direction.
 * @param pixels The frame.
 * @param start Where the the tracing should start from.
 * @param contour_length Max number of traced pixels.
 * @param cw_or_ccw Begin tracing clockwise or counter-clockwise.
 * @return Last point traced.
 */
point2_t radial_sweep(uint8_t (*const pixels)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH], point2_t const start, uint16_t const contour_length, uint8_t const cw_or_ccw);

/**
 * @brief Start a raycast from a @p start position in the direction of @p dir .
 * @param pixels Frame where the raycast will be shot. It needs to be a segmented frame.
 * @param start Where the raycast will begin.
 * @param dir In what direction the ray will be cast.
 * @param callback The function that will get called for every pixel of the cast ray.
 */
void raycast(uint8_t (*const pixels)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH],
             point2_t const start,
             vec2_t const dir,
             uint8_t (*const callback)(uint8_t (*const)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH], point2_t const));

#endif /* _MISC_H_ */