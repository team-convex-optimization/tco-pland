#ifndef _DRAW_H_
#define _DRAW _H_

#include <stdint.h>
#include "tco_shmem.h"
#include "lin_alg.h"

extern const int draw_enabled;

/**
 * @brief Initializes the draw module.
 * @param frame The frame where all shapes will be drawn.
 */
void draw_init(uint8_t (*const frame)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH]);

/**
 * @brief Draws all queued shapes.
 */
void draw_run();

/**
 * @brief Queues a line to be drawn at a given y index.
 * @param row_idx Index of the row where to draw the line
 * @param color Color to fill the line with.
 */
void draw_q_line_horiz(uint16_t const row_idx, uint8_t const color);

/**
 * @brief Queues a square to be drawn. The square will be centered on the provided @p center point.
 * @param center Center of the square.
 * @param size The width of the square.
 * @param color Color of the square.
 */
void draw_q_square(point2_t const center, uint8_t const size, uint8_t const color);

/**
 * @brief Queues a number to be drawn at a given position. @p start refers to the upper left corner.
 * @param number The number that will be drawn.
 * @param start Position where the number will be drawn.
 * @param scale Every pixel of the number will be scaled by this amount in both directions uniformly.
 */
void draw_q_number(uint16_t const number, point2_t const start, uint8_t const scale);

#endif /* _DRAW_H_ */