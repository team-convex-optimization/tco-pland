#ifndef _VECTOR_H_
#define _VECTOR_H_

#include <stdint.h>
#include "tco_shmem.h" /* For Image_bounds defintions */

#define DRAW 1 /* Visual representation of points? */

#define NUM_VECTOR_POINTS 24
#define POINT_TOLERANCE 15
#define ERR_POINT TCO_SIM_WIDTH + 10 /* The value to set `points` when they are not found */
#define SEGMENTATION_DEADZONE 20 /* The segmentation we use can sometimes create a white line inbetween horizontal track lines. This is a search offset. */
#define VECTOR_TOLERANCE 30 /* Number of pixels to cut as 'slack'. The higher the values, the longer the the vectors but less reliable */

typedef struct point_2 {
    uint16_t x;
    uint16_t y;
} point;

typedef struct vector_2 {
    uint8_t valid;
    point bot;
    point top;
} vector;


/**
 * @brief will perform 5 line scans and plot the points
 * @param pixels A segmented image. See `segmentation.h:segment(...)`
 */
void plot_vector_points(uint8_t (*pixels)[TCO_SIM_HEIGHT][TCO_SIM_WIDTH]);

/**
 * @brief will take points and calculate the best suited vector for sides 1 and 2 of `edges`
 * @param target_lines image height at which the correspoding `edge[i]` corresponds to
 * @param edges the list of edges corresponding to `target_lines[i]`. `edges[i][0]` is left, `edges[i][1]` is right.
 * @return A pointer of heap-reserved memory of type vector[2]. 
 */
vector *calculate_vector(uint8_t (*pixels)[TCO_SIM_HEIGHT][TCO_SIM_WIDTH], const uint16_t (*target_lines)[NUM_VECTOR_POINTS], const uint16_t (*edges)[NUM_VECTOR_POINTS][2]);


#endif /* _VECTOR_H_ */