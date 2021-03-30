#ifndef _TRAJECTION_H_
#define _TRAJECTION_H_

#include <stdint.h>
#include "tco_shmem.h" /* For Image_bounds defintions */

#define DRAW 1 /* Visual representation of points? */

/**
 * @brief will perform 5 line scans and plot the points
 * @param pixels A segmented image. See `segmentation.h:segment(...)`
 */
void plot_vector_points(uint8_t (*pixels)[TCO_SIM_HEIGHT][TCO_SIM_WIDTH]);


#endif /* _TRAJECTION_H_ */