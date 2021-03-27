#ifndef _SEGMENTATION_H_
#define _SEGMENTATION_H_

#include <stdint.h>
#include "tco_shmem.h"

/**
 * @brief Segment the image to lines (white (255)) and not-lines (black (0))
 * @param image a grayscale image which segments the image. 
 * @return the image is passed by reference. This reference is modified. 
 */
void segment(uint8_t (*pixels)[TCO_SIM_HEIGHT][TCO_SIM_WIDTH]);

/**
 * @brief Fill in sections of image segmented by white lines
 * @param image a grayscale image which segments the image. 
 * @return the image is passed by reference. This reference is modified. 
 */
// void flood_fill(uint8_t (*pixels)[TCO_SIM_HEIGHT][TCO_SIM_WIDTH], uint16_t x, uint16_t y);

#endif /* _SEGMENTATION_H_ */
