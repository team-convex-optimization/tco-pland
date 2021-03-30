#ifndef _DRAW_H_
#define _DRAW _H_

#include <stdint.h>
#include "tco_shmem.h"

extern const int draw_enabled;

/**
 * @brief Will draw a gray line at a selected row
 * @param pixels an image
 * @param row_idx index of the row where to draw the line
 */
void draw_line_horiz(uint8_t (*const pixels)[TCO_SIM_HEIGHT][TCO_SIM_WIDTH], uint16_t const row_idx);

/**
 * @brief Will draw a square of size `size` on image `pixels` around coordinate (`x`,`y`)
 * @param pixels an image
 * @param pointx x coordinate to draw circle around
 * @param pointy y coordinate to draw circle around
 * @param size the width of the square
 * @param color the grayscale color of the square
 */
void draw_square(uint8_t (*const pixels)[TCO_SIM_HEIGHT][TCO_SIM_WIDTH], uint16_t const pointx, uint16_t const pointy, int const size, uint8_t const color);

/**
 * @brief Draws a number at a given position in an image.
 * @param pixels The image which the number will be drawn on.
 * @param number The number that will be drawn.
 * @param x_start The start x position where the number will be drawn.
 * @param y_start The start y position where the number will be drawn.
 */
void draw_number(uint8_t (*const pixels)[TCO_SIM_HEIGHT][TCO_SIM_WIDTH], uint16_t const number, uint16_t const start_x, uint16_t const start_y);

#endif /* _DRAW_H_ */