#ifndef _TRAJECTION_H_
#define _TRAJECTION_H_

#include <stdio.h>
#include <stdlib.h>

#include <sys/mman.h>

#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <semaphore.h>
#include <errno.h>

#include "tco_shmem.h"
#include "tco_libd.h"


/**
 * @brief Start by finding bottom edges reliably, then move up. After a certain level, use k
 * @param im is a collection of pixels with values 255 OR 0. That is, it is B&W.
 * @param k is the number of neighbors to look at when finding slope. must be odd.
 * @return todo
*/
void k_nearest_neighbor_gradient_center(uint8_t* im, uint8_t k);



#endif /* _TRAJECTION_H_ */