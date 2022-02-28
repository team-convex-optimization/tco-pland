#ifndef _MISC_H_
#define _MISC_H_

/**
 * @brief Collection of common functions and algorithm that don't have internal state and are shared across modules.
 */

#include <stdint.h>
#include "tco_shmem.h"
#include "tco_linalg.h"

typedef uint8_t (*callback_func_t)(uint8_t (*const)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH], point2_t const);

/**
 * @brief General purpose bresenham implementation. It takes in a callback which gets called for
 * every pixel traced by this algorithm.
 * @param pixels A segmented frame.
 * @param pixel_action The user defined callback which gets called for every traced pixel. Should
 * return 0 if tracing should continue and -1 if it should stop. If NULL, it will trace untill the
 * end.
 * @param start Where the line should start.
 * @param end Where the line should end.
 * @note A customized version of https://en.wikipedia.org/wiki/Bresenham%27s_line_algorithm.
 * @return Length of the line.
 */
uint16_t bresenham(uint8_t (*pixels)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH],
                   callback_func_t const pixel_action,
                   point2_t const start,
                   point2_t const end);

/**
 * @brief Perform a fast but rough radial sweep contour trace. It will trace at most @p
 * contour_length pixels and will travel in @p cw_or_ccw (clockwise or counter-clockwise) direction.
 * @param pixels A segmented frame.
 * @param circ_data List of arrays that indicate an offset from the origin at (0,0) to each point on
 * a circle.
 * @param circ_data_len Number of points on the circle.
 * @param start Where the the tracing should start from. It can be on black or white.
 * @param contour_length Max number of traced pixels.
 * @param cw_or_ccw Begin tracing clockwise or counter-clockwise.
 * @param radial_start Where the tracing will begin on the circle. 0=up, 0.25=right, 0.5=down,
 * 0.75=left
 * @param radial_len_max Fraction of points on the circle that can be swept when looking for the
 * next point. If a sweep goes beyong this fraction, it stops
 * @param status Set to 0 if nothing is abnormal, 1 if stopped because swept whole circle without a
 * trace, 2 if stopped when hitting frame bounds, 3 if stopped due to radial len max.
 * @return Last point traced.
 */
point2_t radial_sweep(
    uint8_t (*const pixels)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH],
    vec2_t *const circ_data,
    uint16_t const circ_data_len,
    point2_t const start,
    uint16_t const contour_length,
    uint8_t const cw_or_ccw,
    float const radial_start,
    float const radial_len_max,
    uint8_t *const status);

/**
 * @brief Start a raycast from a @p start position in the direction of @p dir .
 * @param pixels Frame where the raycast will be shot. It needs to be a segmented frame.
 * @param start Where the raycast will begin.
 * @param dir In what direction the ray will be cast.
 * @param callback The function that will get called for every pixel of the cast ray.
 * @return Length of the ray.
 */
uint16_t raycast(uint8_t (*const pixels)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH],
                 point2_t const start,
                 vec2_t const dir,
                 callback_func_t const callback);

/**
 * @brief A callback that draws a 'light' colored pixel and stops at white.
 * @param pixels A segmented frame.
 * @param point Last point of the raycast.
 * @return 0 if cast should continue and -1 if cast should stop.
 */
uint8_t cb_draw_light_stop_white(uint8_t (*const pixels)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH], point2_t const point);

/**
 * @brief A callback that draws a 'light' colored pixel and does not stop at anything.
 * @param pixels A segmented frame.
 * @param point Last point of the raycast.
 * @return 0 if cast should continue and -1 if cast should stop.
 */
uint8_t cb_draw_light_stop_no(uint8_t (*const pixels)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH], point2_t const point);

/**
 * @brief A callback that draws a white pixel on the frame such that it affect further computation
 * and does not stop at anything.
 * @param pixels A segmented frame.
 * @param point Last point of the raycast.
 * @return 0 if cast should continue and -1 if cast should stop.
 */
uint8_t cb_draw_perm_stop_no(uint8_t (*const pixels)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH], point2_t const point);

/**
 * @brief A callback that stops at white and does nothing else.
 * @param pixels A segmented frame.
 * @param point Last point of the raycast.
 * @return 0 if cast should continue and -1 if cast should stop.
 */
uint8_t cb_draw_no_stop_white(uint8_t (*const pixels)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH], point2_t const point);

/**
 * @brief Find the track center in the provided frame.
 * @param pixels The frame where the center will be found. It needs to be a segmented frame.
 * @param bottomr_row_idx Defines the y index in the frame where the center should be found.
 * @param old_center Position from where to start searching
 * @return Track center.
 */
point2_t track_center(uint8_t (*const pixels)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH], uint16_t const bottom_row_idx, uint16_t old_center);

/**
 * @brief Find the track center in the provided frame which is above a black pixel.
 * @param pixels The frame where the center will be found. It needs to be a segmented frame.
 * @param bottomr_row_idx Defines the y index in the frame where the center should be found.
 * @return Point over a black pixel closest to the track center.
 */
point2_t track_center_black(uint8_t (*const pixels)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH], uint16_t const bottom_row_idx);

/**
 * @brief Check if given coordinates lie within a frame.
 * @param x Horizontal coordinate.
 * @param y Vertical coordinate.
 * @return 0 if inside bounds, 1 if not.
 */
uint8_t check_bounds_inside(int16_t x, int16_t y);

#endif /* _MISC_H_ */
