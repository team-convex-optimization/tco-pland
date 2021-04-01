#include "edge_scan.h"
#include "draw.h"

void draw_edges(uint8_t (*pixels)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH], point2_t const (*left_edges)[NUM_LINE_POINTS],
                point2_t const (*right_edges)[NUM_LINE_POINTS], line_t const *lines);
uint8_t diff(uint16_t a, uint16_t b);
void draw_next_way_point(uint8_t (*pixels)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH], line_t const *lines);

/**
 * @brief will perform a line sdcan for left and right points to find track limits. Values are written to left/right_edge
 * @param pixels a segmented image of the track
 * @param center_width the pixel value bewteen 0 and TCO_FRAME_WIDTH -1 to search for the left/right edges
 * @param left_edge a pointer to a point_t for the left side of the track
 * @param right_edge a pointer to a point_t for the right side of the track
 */
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
    draw_next_way_point(pixels, lines);

    free(lines);
}

line_t *edge_calculate(uint8_t (*pixels)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH], point2_t const (*left_edges)[NUM_LINE_POINTS], point2_t const (*right_edges)[NUM_LINE_POINTS])
{
    line_t *lines = calloc(sizeof(line_t), 2);
    point2_t const(*edges)[NUM_LINE_POINTS] = left_edges;
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
                lines[e].bot.x = (*edges)[i].x; /* fill in bot_x */
                lines[e].bot.y = (*edges)[i].y; /* fill in bot_y */
                bot_found = 1;
            }
            else
            {
                /* If the difference in x is < threshold, accept it. Else, save it */
                if (diff((*edges)[i - 1].x, (*edges)[i].x) < LINE_TOLERANCE) //TODO : Also keep track of the slope (increasing/decreasing). This might let us increase lenience.S
                {
                    lines[e].top.x = (*edges)[i].x; /* fill in top_x */
                    lines[e].top.y = (*edges)[i].y; /* fill in top_y */
                    lines[e].valid = 1;             /* WE FOUND A LINE! */
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

/**
 * @brief Draw the track lines and the linescan lines
 * @param left_edges a pointer to an array of size NUM_LINE_POINTS of points on the left side of the track
 * @param right_edges a pointer to an array of size NUM_LINE_POINTS of points on the right side of the track*
 * @param lines  pointer to an array of line_t with 2 lines depicting the track limites
 */
void draw_edges(uint8_t (*pixels)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH], point2_t const (*left_edges)[NUM_LINE_POINTS],
                point2_t const (*right_edges)[NUM_LINE_POINTS], line_t const *lines)
{
    /* Show the target_lines. In this file, left and right edges have the same y */
    for (int i = 0; i < NUM_LINE_POINTS; i++)
    {
        draw_q_line_horiz((*left_edges)[i].y, 50);
    }

    for (uint16_t e = 0; e < 2; e++)
    {
        if (lines[e].valid == 0)
        {
            break;
        }
        // draw_line(pixels, lines[e].bot.x, lines[e].bot.y, lines[e].top.x, lines[e].top.y, 64); //TODO : Use raycast call
        draw_q_square(lines[e].bot, 8, 64);
        draw_q_square(lines[e].top, 8, 64);
    }
}

/**
 * @brief find absolute difference of 2 values
 * @param a value one
 * @param b value two
 * @return read brief
*/
uint8_t diff(uint16_t a, uint16_t b)
{
    return a < b ? b - a : a - b;
}

/**
 * @brief Utility function to draw next best point. 
 * @param pixels an image
 * @param lines A pointer to an array of 2 lines depicting the track limites
*/
void draw_next_way_point(uint8_t (*pixels)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH], line_t const *lines)
{
    line_t left_line = *(lines);
    line_t right_line = *(lines + 1);
    
    line_t avg_point;
    avg_point.bot.x = (left_line.bot.x + right_line.bot.x)/2;
    avg_point.top.x = (left_line.top.x + right_line.top.x)/2;
    avg_point.bot.y = (left_line.bot.y + right_line.bot.y)/2;
    avg_point.top.y = (left_line.top.y + right_line.top.y)/2;

    point2_t way_point;
    way_point.x = (avg_point.top.x + avg_point.bot.x)/2;
    way_point.y = (avg_point.top.y + avg_point.bot.y)/2;
    draw_q_square(way_point, 10, 128);

}