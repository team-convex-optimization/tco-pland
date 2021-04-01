#include "vector.h"
#include "draw.h"

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
    uint16_t const center_width = TCO_FRAME_WIDTH / 2;

    /* Define scanline points */
    uint16_t  target_lines[NUM_VECTOR_POINTS];
    for (int i = 0; i < NUM_VECTOR_POINTS; i++) /* Define the target lines */
    {
        target_lines[i] = ((NUM_VECTOR_POINTS - i + LINE_OFFSET) * TCO_FRAME_HEIGHT) / (LINE_MULTIPLIER * NUM_VECTOR_POINTS);
    }

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
    // if (draw_enabled)
    // {
    //     for (int i = 0; i < NUM_VECTOR_POINTS; i++)
    //     {
    //         draw_line_horiz(pixels, target_lines[i]);
    //     }
    // }

    vector_t *vectors = calculate_vector(pixels, &target_lines, &edges); /* Calculate where the vector is TODO: Make this return the vector*/

    if (draw_enabled)
    {
        for (uint16_t e = 0; e < 2; e++)
        {
            if (vectors[e].valid == 0)
            {
                break;
            }
            // draw_line(pixels, vectors[e].bot.x, vectors[e].bot.y, vectors[e].top.x, vectors[e].top.y, 64); //TODO : Use raycast call
            draw_square(pixels, vectors[e].bot, 8, 64);
            draw_square(pixels, vectors[e].top, 8, 64);
        }
    }

    free(vectors);
}

/* Utility function to find absolute difference of 2 values */
uint8_t diff(uint16_t a, uint16_t b)
{
    return a < b ? b - a : a - b;
}

vector_t *calculate_vector(uint8_t (*pixels)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH], uint16_t const (*target_lines)[NUM_VECTOR_POINTS], uint16_t const (*edges)[NUM_VECTOR_POINTS][2])
{
    vector_t *vectors = calloc(sizeof(vector_t), 2);
    for (uint8_t e = 0; e < 2; e++)
    {
        uint8_t bot_found = 0; /* Keep track if bottom vector has been found */
        for (int8_t i = 0; i < NUM_VECTOR_POINTS; i++)
        {
            if ((*edges)[i][e] == ERR_POINT) /* Skip points that are out of bounds */
            {
                continue;
            }

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