#ifndef _TRAJECTION_H_
#define _TRAJECTION_H_

#include <stdint.h>
#include "tco_shmem.h"
#include "lin_alg.h"

point2_t track_center(uint8_t (*const pixels)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH], uint16_t const bottom_row_idx);

void track_distances(uint8_t (*const pixels)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH], point2_t const center);

#endif /* _TRAJECTION_H_ */