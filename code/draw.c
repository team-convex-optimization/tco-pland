#include "draw.h"

void show_target_lines(uint8_t (*pixels)[TCO_SIM_HEIGHT][TCO_SIM_WIDTH], uint16_t target_line)
{
    for (int i = 0; i < TCO_SIM_WIDTH; i++)
    {
        (*pixels)[target_line][i] = 32;
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