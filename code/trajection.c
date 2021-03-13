#include "trajection.h"
#include <stdio.h>

void show_target_lines(uint8_t (*pixels)[TCO_SIM_HEIGHT][TCO_SIM_WIDTH])
{
    const uint16_t target_line_0 = (TCO_SIM_HEIGHT) / 5;
    const uint16_t target_line_1 = (TCO_SIM_HEIGHT) / 3;
    for (int i = 0; i < TCO_SIM_WIDTH; i++)
    {
        (*pixels)[target_line_0+1][i] = 32;
        (*pixels)[target_line_1+1][i] = 32;
    }
}

void plot_square(uint8_t (*pixels)[TCO_SIM_HEIGHT][TCO_SIM_WIDTH], uint16_t pointx, uint16_t pointy, int size, uint8_t color)
{
    if (pointx < 0 || pointy < 0)
        return;
    const int radius = size/2;
    for (int i = size-radius; i < size+radius; i++)
    {
        uint16_t xpoint = pointx + i - size;
        for (int j = size-radius; j < size+radius; j++)
        {
            uint16_t ypoint = pointy + j - size;
            if (xpoint > TCO_SIM_WIDTH || ypoint > TCO_SIM_WIDTH) /* Bound checking */
                break;

            (*pixels)[ypoint][xpoint] = color;
        }
    }
}


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

void find_targets(uint8_t (*pixels)[TCO_SIM_HEIGHT][TCO_SIM_WIDTH])
{
    const uint16_t target_line_0 = (TCO_SIM_HEIGHT) / 5;
    const uint16_t target_line_1 = (TCO_SIM_HEIGHT) / 3;

    uint16_t top_lines[2], bottom_lines[2];
    /* Find edges */
    find_edges_scan(pixels, target_line_0, &top_lines);
    find_edges_scan(pixels, target_line_1, &bottom_lines);

    /* Paint the edges gray */
    plot_square(pixels, top_lines[0], target_line_0, 10, 128);
    plot_square(pixels, top_lines[1], target_line_0, 10, 192);
    plot_square(pixels, bottom_lines[0], target_line_1, 10, 128);
    plot_square(pixels, bottom_lines[1], target_line_1, 10, 192);
}

void plot_vector_points(uint8_t (*pixels)[TCO_SIM_HEIGHT][TCO_SIM_WIDTH])
{
    //Take scanline for firstrow at bottom. Then go up an increment and spread from same X, looking for next point.
    //Do this and there should be a series of points that are on the line!



}
