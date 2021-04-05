#include <stdlib.h>

#include <string.h>

#include "pre_proc.h"
#include "tco_libd.h"

/**
 * @brief Segment the image to lines (white (255)) and not-lines (black (0))
 * @param image a grayscale image which segments the image. 
 * @return the image is passed by reference. This reference is modified. 
 */
static void segment(uint8_t (*pixels)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH])
{
    uint8_t const delta_threshold = 70;
    uint8_t const look_ahead_length = 4;
    for (uint16_t height_idx = 0; height_idx < TCO_FRAME_HEIGHT; height_idx++)
    {
        for (uint16_t width_idx = 0; width_idx < TCO_FRAME_WIDTH; width_idx++)
        {
            if (width_idx + look_ahead_length < TCO_FRAME_WIDTH &&
                abs((*pixels)[height_idx][width_idx] - (*pixels)[height_idx][width_idx + look_ahead_length]) > delta_threshold)
            {
                (*pixels)[height_idx][width_idx] = 255;
                continue;
            }
            else if (height_idx + look_ahead_length < TCO_FRAME_HEIGHT &&
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

/**
 * @brief Apply a kernel to an image.
 * @param pixels Frame to apply kernel on.
 * @param kernel Kernel to apply. It must be 3x3.
 * @param mult_a What to multiply the source pixel by before adding to computer value.
 * @param mult_b What to multiply computed value by before adding to original pixel.
 * @note Equation for ever pixel is: pixel_new = (pixel * a) + (sum(kern * moore_neighborhood(pixel)) * b)
 */
static void kern3x3_apply(uint8_t (*const pixels)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH], float const kernel[9], float const mult_a, float const mult_b)
{
    uint8_t const row_tmp_count = 2;
    uint8_t row_tmp[2][TCO_FRAME_WIDTH - 2];
    uint8_t row_tmp_idx = 0;
    for (uint16_t y = 1; y < TCO_FRAME_HEIGHT - 1; y++)
    {
        if (y >= 2)
        {
            memcpy(&(*pixels)[y - 2][1], &row_tmp[(row_tmp_idx + row_tmp_count - 1) % row_tmp_count][0], TCO_FRAME_WIDTH - 2);
        }
        for (uint16_t x = 1; x < TCO_FRAME_WIDTH - 1; x++)
        {
            int32_t sum = 0;
            sum += kernel[0] * (*pixels)[y - 1][x - 1];
            sum += kernel[1] * (*pixels)[y - 1][x];
            sum += kernel[2] * (*pixels)[y - 1][x + 1];

            sum += kernel[3] * (*pixels)[y][x - 1];
            sum += kernel[4] * (*pixels)[y][x];
            sum += kernel[5] * (*pixels)[y][x + 1];

            sum += kernel[6] * (*pixels)[y + 1][x - 1];
            sum += kernel[7] * (*pixels)[y + 1][x];
            sum += kernel[8] * (*pixels)[y + 1][x + 1];

            row_tmp[row_tmp_idx][x - 1] = (int32_t)(((*pixels)[y][x] * mult_a) + (sum * mult_b)) % 256;
        }
        row_tmp_idx = (row_tmp_idx + 1) % row_tmp_count;
    }
}

static void dilate(uint8_t (*const pixels)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH], uint8_t const kernel[9])
{
    uint8_t const row_tmp_count = 2;
    uint8_t row_tmp[2][TCO_FRAME_WIDTH - 2];
    uint8_t row_tmp_idx = 0;

    for (uint16_t y = 1; y < TCO_FRAME_HEIGHT - 1; y++)
    {
        if (y >= 2)
        {
            memcpy(&(*pixels)[y - 2][1], &row_tmp[(row_tmp_idx + row_tmp_count - 1) % row_tmp_count][0], TCO_FRAME_WIDTH - 2);
        }
        for (uint16_t x = 1; x < TCO_FRAME_WIDTH - 1; x++)
        {
            uint8_t matching = (kernel[0] & (*pixels)[y - 1][x - 1]) +
                               (kernel[1] & (*pixels)[y - 1][x]) +
                               (kernel[2] & (*pixels)[y - 1][x + 1]) +
                               (kernel[3] & (*pixels)[y][x - 1]) +
                               (kernel[4] & (*pixels)[y][x]) +
                               (kernel[5] & (*pixels)[y][x + 1]) +
                               (kernel[6] & (*pixels)[y + 1][x - 1]) +
                               (kernel[7] & (*pixels)[y + 1][x]) +
                               (kernel[8] & (*pixels)[y + 1][x + 1]);
            row_tmp[row_tmp_idx][x - 1] = matching >= 1 ? 255 : 0;
        }
        row_tmp_idx = (row_tmp_idx + 1) % row_tmp_count;
    }
}

void pre_proc(uint8_t (*const pixels)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH])
{
    uint8_t const kern_1s[9] = {1, 1, 1, 1, 1, 1, 1, 1, 1};

    uint8_t const color_floor = 80; /* 0 - 255 */
    uint8_t const border_size = 1;
    for (uint16_t y = 211; y < TCO_FRAME_HEIGHT; y++)
    {
        memset(&(*pixels)[y][0], color_floor, TCO_FRAME_WIDTH);
    }
    segment(pixels);
    dilate(pixels, kern_1s);
}
