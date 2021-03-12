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

void plot_targets(uint8_t (*pixels)[TCO_SIM_HEIGHT][TCO_SIM_WIDTH]);

#endif /* _TRAJECTION_H_ */