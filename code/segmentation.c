#include <stdlib.h>
#include "segmentation.h"

void segment(uint8_t (*pixels)[TCO_SIM_HEIGHT][TCO_SIM_WIDTH])
{
    uint8_t const delta_threshold = 100;
    uint8_t const look_ahead_length = 4;
    for (uint16_t height_idx = 0; height_idx < TCO_SIM_HEIGHT; height_idx++)
    {
        for (uint16_t width_idx = 0; width_idx < TCO_SIM_WIDTH; width_idx++)
        {
            if (width_idx + look_ahead_length < TCO_SIM_WIDTH &&
                abs((*pixels)[height_idx][width_idx] - (*pixels)[height_idx][width_idx + look_ahead_length]) > delta_threshold)
            {
                (*pixels)[height_idx][width_idx] = 255;
                continue;
            }

            else if (height_idx + look_ahead_length < TCO_SIM_HEIGHT &&
                abs((*pixels)[height_idx][width_idx] - (*pixels)[height_idx + look_ahead_length][width_idx]) > delta_threshold)
            {
                (*pixels)[height_idx][width_idx] = 255;
                continue;
            }

            else 
            {
                (*pixels)[height_idx][width_idx] = 0;
            }
        }
    }
}

/* The below is used for flood_filling the image. */
void flood_fill(uint8_t (*pixels)[TCO_SIM_HEIGHT][TCO_SIM_WIDTH], uint16_t x, uint16_t y) {
    if (y >= TCO_SIM_HEIGHT || x >= TCO_SIM_WIDTH || *pixels == NULL || x == 0 || y == 0)
        return;
    
    if((*pixels)[y][x] == 0)
	{
        (*pixels)[y][x] = 255; /* Set to white */
		flood_fill(pixels, x + 1, y); 
		flood_fill(pixels, x, y + 1);
		flood_fill(pixels, x - 1, y);
		flood_fill(pixels, x, y - 1);
	}
}

