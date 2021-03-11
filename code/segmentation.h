#ifndef _SEGMENTATION_H_
#define _SEGMENTATION_H_

#include <stdint.h>
#include "tco_shmem.h"

void segment(uint8_t (*pixels)[TCO_SIM_HEIGHT][TCO_SIM_WIDTH]);

#endif /* _SEGMENTATION_H_ */
