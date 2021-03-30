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
    for (uint16_t i = 0; i < TCO_SIM_WIDTH; i++)
    {
        (*pixels)[row_idx][i] = 32;
    }
}

void draw_square(uint8_t (*const pixels)[TCO_SIM_HEIGHT][TCO_SIM_WIDTH], uint16_t const point_x, uint16_t const point_y, uint8_t const size, uint8_t const color)
{
    if (!draw_enabled)
    {
        return;
    }
    const uint16_t radius = size / 2;
    uint8_t square_row[size];
    memset(square_row, color, size);

    for (uint16_t y = 0; y < size; y++)
    {
        int32_t const draw_y = point_y - radius;
        if (draw_y + y < 0 || draw_y + y >= TCO_SIM_HEIGHT)
        {
            /* When outside the frame. */
            continue;
        }

        int32_t const draw_x_left_offset = point_x - radius < 0 ? -(point_x - radius) : 0;
        int32_t const draw_x_right_offset = point_x + radius >= TCO_SIM_WIDTH ? (point_x + radius) - TCO_SIM_WIDTH : 0;
        int32_t const draw_x = point_x + draw_x_left_offset - draw_x_right_offset - radius;
        if (draw_x < 0)
        {
            /* When frame is too small for square. */
            continue;
        }
        int32_t const draw_width = size - draw_x_right_offset - draw_x_left_offset;
        if (draw_width < 0)
        {
            /* When frame is too small for square. */
            continue;
        }

        memcpy(&(*pixels)[draw_y + y][draw_x], square_row, draw_width);
    }
}

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