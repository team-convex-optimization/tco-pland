#include "trajection.h"
#include "segmentation.h"
#include "draw.h"
#include <stdio.h>

/* Todo, take center of last frame as new 'center point' here */
void find_edges_scan(uint8_t (*pixels)[TCO_SIM_HEIGHT][TCO_SIM_WIDTH], uint16_t height, uint16_t (*edges)[2])
{
    const uint16_t center_width = TCO_SIM_WIDTH /2;
    for (uint16_t i = center_width; i < TCO_SIM_WIDTH; i++)
    {
        if ((*pixels)[height][i] == 255)
        {
            (*edges)[0] = i; 
            break;
        } 
    }
    for (uint16_t i = center_width; i > 0; i--)
    {
        if ((*pixels)[height][i] == 255)
        {
            (*edges)[1] = i;
            break;
        } 
    }

}

/* Will search for the white line from a given center point */
uint16_t find_edge_around_point(uint8_t (*pixels)[TCO_SIM_HEIGHT][TCO_SIM_WIDTH], uint16_t height, uint16_t center, uint16_t max_spread)
{
    for (int i = 0; i < max_spread; i++)
    {
        if (center+i < TCO_SIM_WIDTH && (*pixels)[height][center+i] == 255)
        {
            return center+i;
        }
        if (center-i > 0 && (*pixels)[height][center-i] == 255)
        {
            return center-i;
        }
    }
    return -1; /* ERROR */
}

void plot_vector_points(uint8_t (*pixels)[TCO_SIM_HEIGHT][TCO_SIM_WIDTH])
{
    /* Take scanline for firstrow at bottom. Then go up an increment and spread from same X, looking for next point.
        Do this and there should be a series of points that are on the line! */
    const uint16_t target_lines[5] = {
        (6 * TCO_SIM_HEIGHT) / 12,  /* Closest to the car */
        (5 * TCO_SIM_HEIGHT) / 12,
        (4 * TCO_SIM_HEIGHT) / 12,
        (3 * TCO_SIM_HEIGHT) / 12,
        (2 * TCO_SIM_HEIGHT) / 12
    };
    const uint16_t max_spread = 100; /* Pixels */
    uint16_t edges[5][2];

    /* Find the bottom line */
    find_edges_scan(pixels, target_lines[0], &edges[0]);

    #ifdef DRAW
    plot_square(pixels, edges[0][0], target_lines[0], 10, 128);
    plot_square(pixels, edges[0][1], target_lines[0], 10, 192);
    #endif

    /* For the remaining edges, we need to find the new points based on the previous points */
    for (int i = 1; i < 5; i++)
    {
        for (int j = 0; j < 2; j++)
        {
            if (edges[i-1][j] == 0) continue;
            edges[i][j] = find_edge_around_point(pixels, target_lines[i], edges[i-1][j], max_spread);
            #ifdef DRAW
            plot_square(pixels, edges[i][j], target_lines[i], 10, 128);
            #endif
        }
    }

    /* Show the target_lines */
    #ifdef DRAW
    for (int i = 0; i < 5; i++)
        show_target_lines(pixels, target_lines[i]);
    #endif
    
}
