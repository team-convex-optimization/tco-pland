#include "vector.h"
#include "segmentation.h"
#include "draw.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void find_edges_scan(uint8_t (*pixels)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH], uint16_t center_width, uint16_t height, uint16_t (*edges)[2])
{
    for (uint16_t i = center_width; i < TCO_FRAME_WIDTH - SEGMENTATION_DEADZONE; i++)
    {
        if ((*pixels)[height][i] == 255)
        {
            (*edges)[0] = i;
            goto right_edge; /* If the edge was found, do not set to err value */
        }
    }
    (*edges)[0] = ERR_POINT;
right_edge:

    for (uint16_t i = center_width; i > 0 + SEGMENTATION_DEADZONE; i--)
    {
        if ((*pixels)[height][i] == 255)
        {
            (*edges)[1] = i;
            return;
        }
    }

    (*edges)[1] = ERR_POINT;
}

void plot_vector_points(uint8_t (*pixels)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH])
{
    /* Take scanline for firstrow at bottom. Then go up an increment and spread from center of last 2 points, looking for next point.
        Do this and there should be a series of points that are on the line! */
    const uint16_t center_width = TCO_FRAME_WIDTH / 2;

    const uint16_t target_lines[NUM_VECTOR_POINTS] = {
        //TODO : Dynamically initialize me.
        (28 * TCO_FRAME_HEIGHT) / 72, /* Closest to the car */
        (27 * TCO_FRAME_HEIGHT) / 72,
        (26 * TCO_FRAME_HEIGHT) / 72,
        (25 * TCO_FRAME_HEIGHT) / 72,
        (24 * TCO_FRAME_HEIGHT) / 72,
        (23 * TCO_FRAME_HEIGHT) / 72,
        (22 * TCO_FRAME_HEIGHT) / 72,
        (21 * TCO_FRAME_HEIGHT) / 72,
        (20 * TCO_FRAME_HEIGHT) / 72,
        (19 * TCO_FRAME_HEIGHT) / 72,
        (18 * TCO_FRAME_HEIGHT) / 72,
        (17 * TCO_FRAME_HEIGHT) / 72,
        (16 * TCO_FRAME_HEIGHT) / 72,
        (15 * TCO_FRAME_HEIGHT) / 72,
        (14 * TCO_FRAME_HEIGHT) / 72,
        (13 * TCO_FRAME_HEIGHT) / 72,
        (12 * TCO_FRAME_HEIGHT) / 72,
        (11 * TCO_FRAME_HEIGHT) / 72,
        (10 * TCO_FRAME_HEIGHT) / 72,
        (9 * TCO_FRAME_HEIGHT) / 72,
        (8 * TCO_FRAME_HEIGHT) / 72,
        (7 * TCO_FRAME_HEIGHT) / 72,
        (6 * TCO_FRAME_HEIGHT) / 72,
        (5 * TCO_FRAME_HEIGHT) / 72,
    };
    uint16_t edges[NUM_VECTOR_POINTS][2];
    /* Find the bottom line */
    find_edges_scan(pixels, center_width, target_lines[0], &edges[0]);

    /* For the remaining edges, we need to find the new points based on the previous points */
    for (int i = 1; i < NUM_VECTOR_POINTS; i++)
    {
        for (int j = 0; j < 2; j++)
        {
            find_edges_scan(pixels, center_width, target_lines[i], &edges[i]);
        }
    }

/* Show the target_lines */
#ifdef DRAW
    for (int i = 0; i < NUM_VECTOR_POINTS; i++)
        show_target_lines(pixels, target_lines[i]);
#endif

    vector *vectors = calculate_vector(pixels, &target_lines, &edges); /* Calculate where the vector is TODO: Make this return the vector*/

#ifdef DRAW /* Plot vector points */
    for (uint16_t e = 0; e < 2; e++)
    {
        if (vectors[e].valid == 0)
            break;
        draw_line_horiz(pixels, vectors[e].bot.x, vectors[e].bot.y, vectors[e].top.x, vectors[e].top.y, 64);
        draw_square(pixels, vectors[e].bot.x, vectors[e].bot.y, 8, 32);
        draw_square(pixels, vectors[e].top.x, vectors[e].top.y, 8, 32);
    }
#endif

    /* TODO : Don't forget to free the vectors! */
    free(vectors);
}

/* Utility function to find absolute difference of 2 values */
uint8_t diff(uint16_t a, uint16_t b)
{
    return a < b ? b - a : a - b;
}

vector *calculate_vector(uint8_t (*pixels)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH], const uint16_t (*target_lines)[NUM_VECTOR_POINTS], const uint16_t (*edges)[NUM_VECTOR_POINTS][2])
{
    vector *vectors = calloc(sizeof(vector), 2);
    for (uint8_t e = 0; e < 2; e++)
    {
        uint8_t bot_found = 0; /* Keep track if bottom vector has been found */
        for (int8_t i = 0; i < NUM_VECTOR_POINTS; i++)
        {
            if ((*edges)[i][e] == ERR_POINT) /* Skip points that are out of bounds */
                continue;

            if (bot_found == 0) /* Bottom not (yet) found */
            {
                vectors[e].bot.x = (*edges)[i][e];     /* fill in bot_x */
                vectors[e].bot.y = (*target_lines)[i]; /* fill in bot_y */
                bot_found = 1;
            }
            else
            {
                /* If the difference in x is < threshold, accept it. Else, save it */
                if (diff((*edges)[i - 1][e], (*edges)[i][e]) < VECTOR_TOLERANCE) //TODO : Also keep track of the slope (increasing/decreasing). This might let us increase lenience.S
                {
                    vectors[e].top.x = (*edges)[i][e];     /* fill in top_x */
                    vectors[e].top.y = (*target_lines)[i]; /* fill in top_y */
                    vectors[e].valid = 1;                  /* WE FOUND A VECTOR! */
                }
                else
                {
                    break; /* This point is not part of the vector! Stop searching */
                }
            }
        }
    }

    return vectors;
}