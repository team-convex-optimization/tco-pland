#ifndef _PRE_PROC_H_
#define _PRE_PROC_H_

#include <stdint.h>
#include "tco_shmem.h"

void pre_proc(uint8_t (*pixels)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH]);

#endif /* _PRE_PROC_H_ */
