#ifndef _LINE_H_
#define _LINE_H_

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <string.h>

#include "tco_shmem.h"
#include "tco_linalg.h"

#include "segmentation.h"
#include "draw.h"

typedef struct line
{
    uint8_t valid;
    point2_t bot;
    point2_t top;
} line_t;

#define NUM_LINE_POINTS 24
#define POINT_OFFSET 4     /* The offset of the numerator to search for lines */
#define POINT_MULTIPLIER 3 /* The multiplier for the denominator */

#define ERR_POINT TCO_FRAME_WIDTH + 10 /* The value to set `points` when they are not found */
#define SEGMENTATION_DEADZONE 20       /* The segmentation we use can sometimes create a white line inbetween horizontal track lines. This is a search offset. */
#define LINE_TOLERANCE 20              /* Number of pixels to cut as 'slack'. The higher the values, the longer the the lines but less reliable */

/**
 * @brief will perform 5 line scans and plot the points
 * @param pixels A segmented image. See `segmentation.h:segment(...)`
 */
void edge_plot(uint8_t (*pixels)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH]);

/**
 * @brief will take points and calculate the best suited line for sides 1 and 2 of `edges`
 * @param left_edges a list of points corresponding to left_edges
 * @param right_edges a list of points corresponding to right_edges
 * @return A pointer of heap-reserved memory of type line_t[2]. 
 */
line_t *edge_calculate(uint8_t (*pixels)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH], point2_t const (*left_edges)[NUM_LINE_POINTS], point2_t const (*right_edges)[NUM_LINE_POINTS]);

#endif /* _LINE_H_ */