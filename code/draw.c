#include <string.h>
#include <stdio.h>

#include "draw.h"
#include "tco_libd.h"

void draw_horiz_line(uint8_t (*const pixels)[TCO_SIM_HEIGHT][TCO_SIM_WIDTH], uint16_t const row_idx)
{
    if (!draw_enabled)
    {
        return;
    }
    for (int i = 0; i < TCO_SIM_WIDTH; i++)
    {
        (*pixels)[row_idx][i] = 32;
    }
}

void draw_square(uint8_t (*const pixels)[TCO_SIM_HEIGHT][TCO_SIM_WIDTH], uint16_t const point_x, uint16_t const point_y, int const size, uint8_t const color)
{
    if (!draw_enabled)
    {
        return;
    }
    const int radius = size / 2;
    for (int i = size - radius; i < size + radius; i++)
    {
        uint16_t point_x_cur = point_x + i - size;
        for (int j = size - radius; j < size + radius; j++)
        {
            uint16_t point_y_cur = point_y + j - size;
            if (point_x_cur > TCO_SIM_WIDTH || point_y_cur > TCO_SIM_WIDTH) /* Bound checking */
                break;

            (*pixels)[point_y_cur][point_x_cur] = color;
        }
    }
}

/**
 * @brief Draws a number at a given position in an image.
 * @param pixels The image which the number will be drawn on.
 * @param number The number that will be drawn.
 * @param x_start The start x position where the number will be drawn.
 * @param y_start The start y position where the number will be drawn.
 */
void draw_number(uint8_t (*const pixels)[TCO_SIM_HEIGHT][TCO_SIM_WIDTH], uint16_t const number, uint16_t const start_x, uint16_t const start_y)
{
    if (!draw_enabled)
    {
        return;
    }
    static const uint8_t digit_scale = 4;   /* How much to scale each digit (scaling is done uniformly in x and y axis). */
    static const uint8_t digit_spacing = 4; /* Distance between digits horizontally. */
    static const uint8_t digit_num_max = 5; /* Passed number is limited in size by the uint16_t type. */
    static const uint8_t digit_width = 4;
    static const uint8_t digit_height = 7;
    /* Each digit is described by a 4 by 7 array of pixels where 1 means white and 0 means black. */
    static const uint8_t digit_pixels[10][4 * 7] = {
        {1, 1, 1, 1,
         1, 0, 0, 1,
         1, 0, 0, 1,
         1, 0, 0, 1,
         1, 0, 0, 1,
         1, 0, 0, 1,
         1, 1, 1, 1},
        {0, 1, 1, 0,
         0, 0, 1, 0,
         0, 0, 1, 0,
         0, 0, 1, 0,
         0, 0, 1, 0,
         0, 0, 1, 0,
         1, 1, 1, 1},
        {1, 1, 1, 1,
         0, 0, 0, 1,
         0, 0, 0, 1,
         1, 1, 1, 1,
         1, 0, 0, 0,
         1, 0, 0, 0,
         1, 1, 1, 1},
        {1, 1, 1, 1,
         0, 0, 0, 1,
         0, 0, 0, 1,
         1, 1, 1, 1,
         0, 0, 0, 1,
         0, 0, 0, 1,
         1, 1, 1, 1},
        {1, 0, 0, 1,
         1, 0, 0, 1,
         1, 0, 0, 1,
         1, 1, 1, 1,
         0, 0, 0, 1,
         0, 0, 0, 1,
         0, 0, 0, 1},
        {1, 1, 1, 1,
         1, 0, 0, 0,
         1, 0, 0, 0,
         1, 1, 1, 1,
         0, 0, 0, 1,
         0, 0, 0, 1,
         1, 1, 1, 1},
        {1, 1, 1, 1,
         1, 0, 0, 0,
         1, 0, 0, 0,
         1, 1, 1, 1,
         1, 0, 0, 1,
         1, 0, 0, 1,
         1, 1, 1, 1},
        {1, 1, 1, 1,
         0, 0, 0, 1,
         0, 0, 0, 1,
         0, 0, 0, 1,
         0, 0, 0, 1,
         0, 0, 0, 1,
         0, 0, 0, 1},
        {1, 1, 1, 1,
         1, 0, 0, 1,
         1, 0, 0, 1,
         1, 1, 1, 1,
         1, 0, 0, 1,
         1, 0, 0, 1,
         1, 1, 1, 1},
        {1, 1, 1, 1,
         1, 0, 0, 1,
         1, 0, 0, 1,
         1, 1, 1, 1,
         0, 0, 0, 1,
         0, 0, 0, 1,
         1, 1, 1, 1},
    };

    char num_str[digit_num_max];
    sprintf((char *)&num_str, "%u", number);

    uint8_t digit_num = 0;
    for (uint8_t num_str_idx = 0; num_str_idx < digit_num_max; num_str_idx++)
    {
        if (num_str[num_str_idx] == '\0')
        {
            break; /* No more digits. */
        }
        digit_num += 1;
    }

    uint16_t const scanline_width = digit_num * ((digit_scale * digit_width) + digit_spacing);
    for (uint8_t scanline_y = 0; scanline_y < digit_height; scanline_y++)
    {
        uint8_t scanline[scanline_width];
        memset(&(scanline[0]), 0, scanline_width);
        uint16_t scanline_progress = 0;

        for (uint8_t digit_idx = 0; digit_idx < digit_num; digit_idx++)
        {
            memset(&(scanline[scanline_progress]), 0, digit_spacing);
            scanline_progress += digit_spacing;

            uint8_t digit = num_str[digit_idx] - '0';
            for (uint8_t pixel_idx_x = 0; pixel_idx_x < digit_width; pixel_idx_x++)
            {
                memset(&scanline[scanline_progress], digit_pixels[digit][(digit_width * scanline_y) + pixel_idx_x] * 255, digit_scale);
                scanline_progress += digit_scale;
            }
        }
        for (uint8_t row_n = 0; row_n < digit_scale; row_n++)
        {
            memcpy(&(*pixels)[start_y + (scanline_y * digit_scale) + row_n][start_x], &scanline, scanline_width);
        }
    }
}