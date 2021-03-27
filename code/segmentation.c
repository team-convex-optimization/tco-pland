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

/*
 Will return a pixel at position (x,y). 
*/
uint8_t *get_pixel(uint8_t (*pixels)[TCO_SIM_HEIGHT][TCO_SIM_WIDTH], uint16_t x, uint16_t y) {
    if (y < 0 || y > TCO_SIM_HEIGHT || x < 0 || x > TCO_SIM_WIDTH)
        return NULL;
    return pixels[y][x];
}

uint8_t get_pixel_color(uint8_t (*pixels)[TCO_SIM_HEIGHT][TCO_SIM_WIDTH], uint16_t x, uint16_t y) {
    if (y < 0 || y > TCO_SIM_HEIGHT || x < 0 || x > TCO_SIM_WIDTH)
        return NULL; //error
    return (*pixels)[y][x];
}

void set_pixel(uint8_t (*pixels)[TCO_SIM_HEIGHT][TCO_SIM_WIDTH], uint16_t x, uint16_t y, uint8_t color) {
    // uint8_t *pixel = get_pixel(pixels, x, y);
    // *pixel = color;
    (*pixels)[y][x] = color;
}

void flood_fill(uint8_t (*pixels)[TCO_SIM_HEIGHT][TCO_SIM_WIDTH], uint16_t x, uint16_t y) {
    /* Intuition: The idea here is to use Breadth First Search to start exploring which nodes need to be filled. */
    if (y < 0 || y >= TCO_SIM_HEIGHT || x < 0 || x >= TCO_SIM_WIDTH)
    {
        return;
    }
    
    if(get_pixel_color(pixels, x, y) == 0)
	{
        set_pixel(pixels, x, y, 255);
		flood_fill(pixels, x+1, y); /* Reccursivly search the rest of the track */
		flood_fill(pixels, x, y+1);
		flood_fill(pixels, x-1, y);
		flood_fill(pixels, x, y-1);
	}

    return; /* IMPLEMENT ME */
}