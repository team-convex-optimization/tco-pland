#ifndef _DRAW_H_
#define _DRAW _H_

#include <stdint.h>
#include "tco_shmem.h"
#include "lin_alg.h"

extern const int draw_enabled;

/**
 * @brief Will draw a gray line at a selected row
 * @param pixels The target image.
 * @param row_idx Index of the row where to draw the line
 */
void draw_line_horiz(uint8_t (*const pixels)[TCO_SIM_HEIGHT][TCO_SIM_WIDTH], uint16_t const row_idx);

/**
 * @brief Will draw a square of size `size` on image `pixels` around coordinate (`x`,`y`)
 * @param pixels The target image.
 * @param point Center of the square.
 * @param size The width of the square.
 * @param color Color of the square.
 */
void draw_square(uint8_t (*const pixels)[TCO_SIM_HEIGHT][TCO_SIM_WIDTH], point2_t const point, uint8_t const size, uint8_t const color);

/**
 * @brief Draws a number at a given position in an image.
 * @param pixels The target image.
 * @param number The number that will be drawn.
 * @param start Position where the number will be drawn.
 */
void draw_number(uint8_t (*const pixels)[TCO_SIM_HEIGHT][TCO_SIM_WIDTH], uint16_t const number, point2_t const start);

#endif /* _DRAW_H_ */