#ifndef _TRAJECTION_H_
#define _TRAJECTION_H_

#include <stdint.h>
#include "tco_shmem.h" /* For Image_bounds defintions */

void track_center(uint8_t (*pixels)[TCO_SIM_HEIGHT][TCO_SIM_WIDTH], uint16_t bottom_row_idx);

void track_distances(uint8_t (*pixels)[TCO_SIM_HEIGHT][TCO_SIM_WIDTH], uint16_t bottom_row_idx);

#endif /* _TRAJECTION_H_ */