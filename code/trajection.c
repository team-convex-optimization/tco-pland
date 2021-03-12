#include "trajection.h"
#include <stdio.h>
void show_target_lines(uint8_t (*pixels)[TCO_SIM_HEIGHT][TCO_SIM_WIDTH])
{
    const uint16_t target_line_0 = (TCO_SIM_HEIGHT) / 5;
    const uint16_t target_line_1 = (TCO_SIM_HEIGHT) / 3;
    for (int i = 0; i < TCO_SIM_WIDTH; i++)
    {
        (*pixels)[target_line_1][i] = 255;
        (*pixels)[target_line_0][i] = 255;

    }
}
