#ifndef _LINE_H_
#define _LINE_H_

#include <stdint.h>
#include "tco_shmem.h" /* For Image_bounds defintions */
#include "utils/lin_alg.h" /* For point and line typedefs */
#include "segmentation.h"
#include "draw.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define NUM_LINE_POINTS 24

/*  For the line scan technique, equidistant lines are taken and used for the creation of the edges
    Each lines is plotted as follows:
        target_lines[i] = ((NUM_LINE_POINTS - i + POINT_OFFSET) * TCO_FRAME_HEIGHT) / (POINT_MULTIPLIER * NUM_LINE_POINTS);
    where `i` is an iterator between 0 and NUM_LINE_POINTS - 1; */
#define POINT_OFFSET 4 /* The offset of the numerator to search for lines */
#define POINT_MULTIPLIER 3 /* The multiplier for the denominator */

#define ERR_POINT TCO_FRAME_WIDTH + 10 /* The value to set `points` when they are not found */
#define SEGMENTATION_DEADZONE 20       /* The segmentation we use can sometimes create a white line inbetween horizontal track lines. This is a search offset. */
#define LINE_TOLERANCE 30            /* Number of pixels to cut as 'slack'. The higher the values, the longer the the lines but less reliable */

/**
 * @brief will perform 5 line scans and plot the points
 * @param pixels A segmented image. See `segmentation.h:segment(...)`
 */
void plot_line_points(uint8_t (*pixels)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH]);

/**
 * @brief will take points and calculate the best suited line for sides 1 and 2 of `edges`
 * @param target_lines image height at which the correspoding `edge[i]` corresponds to
 * @param edges the list of edges corresponding to `target_lines[i]`. `edges[i][0]` is left, `edges[i][1]` is right.
 * @return A pointer of heap-reserved memory of type line[2]. 
 */
line_t *calculate_line(uint8_t (*pixels)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH], uint16_t const (*target_lines)[NUM_LINE_POINTS], uint16_t const (*edges)[NUM_LINE_POINTS][2]);

#endif /* _LINE_H_ */