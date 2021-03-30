#ifndef _DRAW_H_
#define _DRAW _H_

#include <stdint.h>
#include "tco_shmem.h" /* For Image_bounds defintions */

/**
 * @brief Will draw a gray line at pixel row Height
 * @param pixels an image
 */
void show_target_lines(uint8_t (*pixels)[TCO_SIM_HEIGHT][TCO_SIM_WIDTH], uint16_t target_line);

/**
 * @brief Will draw a square of size `size` on image `pixels` around coordinate (`x`,`y`)
 * @param pixels an image
 * @param pointx x coordinate to draw circle around
 * @param pointy y coordinate to draw circle around
 * @param size the width of the square
 * @param color the grayscale color of the square
 */
void plot_square(uint8_t (*pixels)[TCO_SIM_HEIGHT][TCO_SIM_WIDTH], uint16_t pointx, uint16_t pointy, int size, uint8_t color);

#endif /* _DRAW_H_ */