#include "trajection.h"
#include <stdio.h>
void show_target_lines(uint8_t (*pixels)[TCO_SIM_HEIGHT][TCO_SIM_WIDTH])
{
    const uint16_t target_line_0 = (TCO_SIM_HEIGHT) / 5;
    const uint16_t target_line_1 = (TCO_SIM_HEIGHT) / 3;
    for (int i = 0; i < TCO_SIM_WIDTH; i++)
    {
        (*pixels)[target_line_0][i] = 32;
        (*pixels)[target_line_1][i] = 32;
    }
}

void plot_square(uint8_t (*pixels)[TCO_SIM_HEIGHT][TCO_SIM_WIDTH], uint16_t pointx, uint16_t pointy, int size, uint8_t color)
{
    const int radius = size/2;
    for (int i = size-radius; i < size+radius; i++)
    {
        uint16_t xpoint = pointx + i;
        for (int j = size-radius; j < size+radius; j++)
        {
            uint16_t ypoint = pointy + j;
            if (xpoint > TCO_SIM_WIDTH || ypoint > TCO_SIM_WIDTH) /* Bound checking */
                break;

            (*pixels)[ypoint][xpoint] = color;
        }
    }
}


void plot_targets(uint8_t (*pixels)[TCO_SIM_HEIGHT][TCO_SIM_WIDTH])
{
    const uint16_t target_line_0 = (TCO_SIM_HEIGHT) / 5;
    const uint16_t target_line_1 = (TCO_SIM_HEIGHT) / 3;
    const uint16_t center_width = TCO_SIM_WIDTH /2;
    const uint8_t segmented_line_width = 5;

    uint16_t top_line_l, top_line_r, bot_line_l, bot_line_r = 0;
    uint8_t switch_lines = 0; /* Bool */

    /* Find edges for target_line_0 */
    for (int i = 0; i < TCO_SIM_WIDTH; i++)
    {
        if ((*pixels)[target_line_0][i] == 255)
        {
            if (switch_lines == 0)
            {
                switch_lines++;
                top_line_l = i;
                i += segmented_line_width;
            } 
            else 
            {
                top_line_r = i;
            }
        }
    }

    switch_lines = 0;
    /* Find edges for target_line_1 */
    for (int i = 0; i < TCO_SIM_WIDTH; i++)
    {
        if ((*pixels)[target_line_1][i] == 255)
        {
            if (switch_lines == 0)
            {
                switch_lines++;
                bot_line_l = i;
                i += segmented_line_width;
            } 
            else 
            {
                bot_line_r = i;
            }
        }
    }

    /* Paint the edges gray */
    plot_square(pixels, top_line_l, target_line_0, 10, 128);
    plot_square(pixels, top_line_r, target_line_0, 10, 192);

    plot_square(pixels, bot_line_l, target_line_1, 10, 128);
    plot_square(pixels, bot_line_r, target_line_1, 10, 192);
}
