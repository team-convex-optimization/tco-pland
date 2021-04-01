#include "edge_scan.h"
#include "draw.h"

/****************************
 * UTILITY FUNCTION PROTOTYPES
 ****************************/
void draw_edges(uint8_t (*pixels)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH], point2_t const (*left_edges)[NUM_LINE_POINTS], 
                    point2_t const (*right_edges)[NUM_LINE_POINTS], line_t const *lines);
uint8_t diff(uint16_t a, uint16_t b);

/****************************
 * LINE FINDING FUNCTIONS
 ****************************/
void edge_scan(uint8_t (*pixels)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH], uint16_t center_width, point2_t *left_edge, point2_t *right_edge)
{
    for (uint16_t i = center_width; i < TCO_FRAME_WIDTH - SEGMENTATION_DEADZONE; i++)
    {
        if ((*pixels)[left_edge->y][i] == 255)
        {
            left_edge->x = i;
            goto right_edge; /* If the edge was found, do not set to err value */
        }
    }
    left_edge->x = ERR_POINT;
right_edge:

    for (uint16_t i = center_width; i > 0 + SEGMENTATION_DEADZONE; i--)
    {
        if ((*pixels)[right_edge->y][i] == 255)
        {
            right_edge->x = i;
            return;
        }
    }

    right_edge->y = ERR_POINT;
}

void edge_plot(uint8_t (*pixels)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH])
{
    uint16_t const center_width = TCO_FRAME_WIDTH / 2;
    point2_t left_edges[NUM_LINE_POINTS], right_edges[NUM_LINE_POINTS];

    /* Define scanline points */
    uint16_t target_lines[NUM_LINE_POINTS];
    for (int i = 0; i < NUM_LINE_POINTS; i++) /* Define the target lines */
    {
        target_lines[i] = ((NUM_LINE_POINTS - i + POINT_OFFSET) * TCO_FRAME_HEIGHT) / (POINT_MULTIPLIER * NUM_LINE_POINTS);
        left_edges[i].y = target_lines[i];
        right_edges[i].y = target_lines[i];
    }

    /* Find the bottom line */
    edge_scan(pixels, center_width, &left_edges[0], &right_edges[0]);

    /* For the remaining edges, we need to find the new points based on the previous points */
    for (int i = 1; i < NUM_LINE_POINTS; i++)
    {
        for (int j = 0; j < 2; j++)
        {
            edge_scan(pixels, center_width, &left_edges[i], &right_edges[i]);
        }
    }

    /* Calculate where the line is TODO: Make this return the line*/
    line_t *lines = edge_calculate(pixels, &left_edges, &right_edges); 

    /* Draw the lines */
    draw_edges(pixels, &left_edges, &right_edges, lines);
    
    free(lines);
}

line_t *edge_calculate(uint8_t (*pixels)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH], point2_t const (*left_edges)[NUM_LINE_POINTS], point2_t const (*right_edges)[NUM_LINE_POINTS])
{
    line_t *lines = calloc(sizeof(line_t), 2);
    point2_t const (*edges)[NUM_LINE_POINTS] = left_edges;
    for (uint8_t e = 0; e < 2; e++)
    {
        uint8_t bot_found = 0; /* Keep track if bottom line has been found */
        for (int8_t i = 0; i < NUM_LINE_POINTS; i++)
        {
            if ((*edges)[i].x == ERR_POINT) /* Skip points that are out of bounds */
            {
                continue;
            }

            if (bot_found == 0) /* Bottom not (yet) found */
            {
                lines[e].bot.x = (*edges)[i].x;     /* fill in bot_x */
                lines[e].bot.y = (*edges)[i].y; /* fill in bot_y */
                bot_found = 1;
            }
            else
            {
                /* If the difference in x is < threshold, accept it. Else, save it */
                if (diff((*edges)[i - 1].x, (*edges)[i].x) < LINE_TOLERANCE) //TODO : Also keep track of the slope (increasing/decreasing). This might let us increase lenience.S
                {
                    lines[e].top.x = (*edges)[i].x;     /* fill in top_x */
                    lines[e].top.y = (*edges)[i].y; /* fill in top_y */
                    lines[e].valid = 1;                  /* WE FOUND A LINE! */
                }
                else
                {
                    break; /* This point is not part of the line! Stop searching */
                }
            }
        }
        edges = right_edges; /* Swap sides */
    }

    return lines;
}


/****************************
 * UTILITY FUNCTION DEFINTIONS
 ****************************/

/* Utitlity function to draw the points */
void draw_edges(uint8_t (*pixels)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH], point2_t const (*left_edges)[NUM_LINE_POINTS], 
                    point2_t const (*right_edges)[NUM_LINE_POINTS], line_t const *lines)
{
    /* Show the target_lines. In this file, left and right edges have the same y */
    for (int i = 0; i < NUM_LINE_POINTS; i++)
    {
        draw_line_horiz(pixels, (*left_edges)[i].y);
    }

    for (uint16_t e = 0; e < 2; e++)
    {
        if (lines[e].valid == 0)
        {
            break;
        }
        // draw_line(pixels, lines[e].bot.x, lines[e].bot.y, lines[e].top.x, lines[e].top.y, 64); //TODO : Use raycast call
        draw_square(pixels, lines[e].bot, 8, 64);
        draw_square(pixels, lines[e].top, 8, 64);
    }

}

/* Utility function to find absolute difference of 2 values */
uint8_t diff(uint16_t a, uint16_t b)
{
    return a < b ? b - a : a - b;
}