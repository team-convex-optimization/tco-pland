#include <stdlib.h>

#include <string.h>

#include "tco_libd.h"
#include "tco_linalg.h"

#include "buf_circ.h"
#include "pre_proc.h"
#include "draw.h"
#include "misc.h"
#include "stack_dyna.h"

typedef struct region
{
    line2_t region;
    uint16_t size;
} region_t;

static uint16_t const frame_bot = 210; /* Where the usable frame ends in the y direction from the top. */

/**
 * @brief algo_segment the image to lines (white (255)) and not-lines (black (0))
 * @param image a grayscale image which algo_segments the image. 
 * @return the image is passed by reference. This reference is modified. 
 */
static void algo_segment(uint8_t (*pixels)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH])
{
    uint8_t const delta_threshold = 30;
    uint8_t const look_ahead_length = 10;
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

            if (height_idx + look_ahead_length < TCO_FRAME_HEIGHT &&
                abs((*pixels)[height_idx][width_idx] - (*pixels)[height_idx + look_ahead_length][width_idx]) > delta_threshold)
            {
                (*pixels)[height_idx][width_idx] = 255;
                continue;
            }

            (*pixels)[height_idx][width_idx] = 0;
        }
    }
}

static void morph_primitive(uint8_t (*const pixels)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH], uint8_t const dilate_or_erode, uint8_t const three_or_five)
{
    uint8_t const row_tmp_count = (three_or_five * 2 + !three_or_five * 3);
    uint8_t row_tmp[row_tmp_count][TCO_FRAME_WIDTH - 2];
    uint8_t row_tmp_idx = 0;

    for (uint16_t y = 1; y < TCO_FRAME_HEIGHT - 1; y++)
    {
        if (y >= row_tmp_count)
        {
            memcpy(&(*pixels)[y - row_tmp_count][1], &row_tmp[(row_tmp_idx + row_tmp_count - 1) % row_tmp_count][0], TCO_FRAME_WIDTH - 2);
        }
        for (uint16_t x = 1; x < TCO_FRAME_WIDTH - 1; x++)
        {
            uint8_t matching;
            if (three_or_five)
            {
                uint8_t const row1 = (1 & (*pixels)[y - 1][x - 1]) + (1 & (*pixels)[y - 1][x]) + (1 & (*pixels)[y - 1][x + 1]);
                uint8_t const row2 = (1 & (*pixels)[y][x - 1]) + (1 & (*pixels)[y][x]) + (1 & (*pixels)[y][x + 1]);
                uint8_t const row3 = (1 & (*pixels)[y + 1][x - 1]) + (1 & (*pixels)[y + 1][x]) + (1 & (*pixels)[y + 1][x + 1]);
                matching = row1 + row2 + row3;
            }
            else
            {
                uint8_t const row1 = (1 & (*pixels)[y - 2][x - 2]) + (1 & (*pixels)[y - 2][x - 1]) + (1 & (*pixels)[y - 2][x]) + (1 & (*pixels)[y - 2][x + 1]) + (1 & (*pixels)[y - 2][x + 2]);
                uint8_t const row2 = (1 & (*pixels)[y - 1][x - 2]) + (1 & (*pixels)[y - 1][x - 1]) + (1 & (*pixels)[y - 1][x]) + (1 & (*pixels)[y - 1][x + 1]) + (1 & (*pixels)[y - 1][x + 2]);
                uint8_t const row3 = (1 & (*pixels)[y][x - 2]) + (1 & (*pixels)[y][x - 1]) + (1 & (*pixels)[y][x]) + (1 & (*pixels)[y][x + 1]) + (1 & (*pixels)[y][x + 2]);
                uint8_t const row4 = (1 & (*pixels)[y + 1][x - 2]) + (1 & (*pixels)[y + 1][x - 1]) + (1 & (*pixels)[y + 1][x]) + (1 & (*pixels)[y + 1][x + 1]) + (1 & (*pixels)[y + 1][x + 2]);
                uint8_t const row5 = (1 & (*pixels)[y + 2][x - 2]) + (1 & (*pixels)[y + 2][x - 1]) + (1 & (*pixels)[y + 2][x]) + (1 & (*pixels)[y + 2][x + 1]) + (1 & (*pixels)[y + 2][x + 2]);
                matching = row1 + row2 + row3 + row4 + row5;
            }

            /* Branchless selection of dilation or erosion. */
            row_tmp[row_tmp_idx][x - 1] = (dilate_or_erode * (matching >= 1)) + (!dilate_or_erode * (matching == (9 * three_or_five + 25 * !three_or_five))) ? 255 : 0;
        }
        row_tmp_idx = (row_tmp_idx + 1) % row_tmp_count;
    }
}

static void algo_grating(uint8_t (*const pixels)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH])
{
    for (uint16_t y = 30; y < 211 - 10; y += 10)
    {
        uint8_t ray_num = 0;
        uint16_t border_size = 0;
        for (uint16_t x = 0; x < TCO_FRAME_WIDTH; x++)
        {
            if ((*pixels)[y][x] == 255)
            {
                border_size++;
                draw_q_pixel((point2_t){x, y}, 60);
            }
            else if ((*pixels)[y][x] == 0)
            {
                point2_t start = {x, y};
                uint16_t ray_len = raycast(pixels, start, (vec2_t){1, 0}, &cb_draw_no_stop_white);
                x += ray_len;
                point2_t end = {x, y};

                if (ray_len > 16 && border_size > 4 && border_size < 150)
                {
                    if (!((ray_num + 1) % 2 == 0))
                    {
                        draw_q_square(start, 4, 120);
                        draw_q_square(end, 4, 100);
                        draw_q_square((point2_t){(end.x + start.x) / 2, (end.y + start.y) / 2}, 4, 200);
                        bresenham(pixels, &cb_draw_light_stop_no, start, end);
                    }
                    ray_num++;
                }
                else
                {
                    border_size += ray_len;
                }
                border_size = 0;
            }
        }
    }
}

static void span_fill(uint8_t (*const pixels)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH], point2_t const origin)
{
    if ((*pixels)[origin.y][origin.x] == 255)
    {
        return;
    }

    point2_t pt;
    uint16_t x1;
    uint8_t span_above, span_below;

    stack_dyna_t stack = {NULL, 0, 0, sizeof(point2_t)};

    /* TODO: Finish this. */

    if (stack.data != NULL)
    {
        free(stack.data);
    }
}

void pre_proc(uint8_t (*const pixels)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH])
{
    uint8_t const color_floor = (*pixels)[180][TCO_FRAME_WIDTH / 2]; /* 0 - 255 */
    uint8_t const border_size = 10;

    for (uint16_t y = 0; y < TCO_FRAME_HEIGHT; y++)
    {
        uint8_t const color_floor_adaptive = color_floor - ((y / (float)TCO_FRAME_HEIGHT) * color_floor);
        if (y < border_size || y > frame_bot)
        {
            memset(&(*pixels)[y][0], color_floor_adaptive, TCO_FRAME_WIDTH);
        }
        else
        {
            memset(&(*pixels)[y][0], color_floor_adaptive, border_size);
            memset(&(*pixels)[y][TCO_FRAME_WIDTH - border_size], color_floor_adaptive, border_size);
        }
    }
    algo_segment(pixels);
//    morph_primitive(pixels, 0, 1); /* Erode 3x3 */
    morph_primitive(pixels, 1, 0); /* Dilate 5x5 */
//    morph_primitive(pixels, 0, 0); /* Dilate 3x3 */
//    morph_primitive(pixels, 0, 1); /* Erode 3x3 */
//    morph_primitive(pixels, 1, 0); /* Dilate 5x5 */
}
