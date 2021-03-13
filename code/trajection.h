#ifndef _TRAJECTION_H_
#define _TRAJECTION_H_

#include <stdint.h>
#include "tco_shmem.h" /* For Image_bounds defintions */

/**
 * @brief Will plot a target of where the next optimal point is for the car to be
 * @param image A segmented image. See `segmentation.h:segment(...)`
 * @return the image is passed by reference. This reference is modified. 
 */
void show_target_lines(uint8_t (*pixels)[TCO_SIM_HEIGHT][TCO_SIM_WIDTH]);

/**
 * @brief Will plot squares on the image to help visualize trajection(s)
 * @param image A grayscale image to apply the points too
 * @param pointx The pixel width the point is at
 * @param pointy The pixel Height the point is at
 * @param size The size the square should be around (pointx, pointy)
 * @param color The grayscale color the square should be
 * @return the image is passed by reference. This reference is modified. 
 */
void plot_square(uint8_t (*pixels)[TCO_SIM_HEIGHT][TCO_SIM_WIDTH], uint16_t pointx, uint16_t pointy, int size, uint8_t color);


void find_targets(uint8_t (*pixels)[TCO_SIM_HEIGHT][TCO_SIM_WIDTH]);

#endif /* _TRAJECTION_H_ */