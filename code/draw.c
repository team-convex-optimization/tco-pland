#include <string.h>
#include <stdio.h>

#include "draw.h"
#include "tco_libd.h"

static uint8_t (*target_frame)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH];

/* Types used in the draw queue. */
typedef struct line_horiz
{
    uint16_t y;
    uint8_t color;
} line_horiz_t;
typedef struct square
{
    point2_t center;
    uint8_t size;
    uint8_t color;
} square_t;
typedef struct number
{
    point2_t start;
    uint16_t number;
    uint8_t scale;
} number_t;
typedef struct pixel
{
    point2_t pos;
    uint8_t color;
} pixel_t;

static const uint16_t queue_size_line_horiz = 256;
static line_horiz_t queue_line_horiz[queue_size_line_horiz] = {0};
static uint16_t queue_idx_line_horiz = 0;

static const uint16_t queue_size_square = 256;
static square_t queue_square[queue_size_square] = {0};
static uint16_t queue_idx_square = 0;

static const uint16_t queue_size_number = 256;
static number_t queue_number[queue_size_number] = {0};
static uint16_t queue_idx_number = 0;

static const uint16_t queue_size_pixel = 2096;
static pixel_t queue_pixel[queue_size_pixel] = {0};
static uint16_t queue_idx_pixel = 0;

static void draw_line_horiz(uint16_t const row_idx, uint8_t const color)
{
    memset((*target_frame)[row_idx], color, TCO_FRAME_WIDTH);
}

static void draw_square(point2_t const point, uint8_t const size, uint8_t const color)
{
    const uint16_t radius = size / 2;
    uint8_t square_row[size];
    memset(square_row, color, size);

    for (uint16_t scanline_n = 0; scanline_n < size; scanline_n++)
    {
        int32_t const draw_y = point.y - radius + scanline_n;
        if (draw_y < 0 || draw_y >= TCO_FRAME_HEIGHT)
        {
            /* When outside the frame. */
            continue;
        }

        int32_t const draw_x_left_offset = point.x - radius < 0 ? -(point.x - radius) : 0;
        int32_t const draw_x_right_offset = point.x + radius >= TCO_FRAME_WIDTH ? (point.x + radius) - TCO_FRAME_WIDTH : 0;
        int32_t const draw_x = point.x - radius + draw_x_left_offset; /* TODO: Verify if this is correct. */
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

        memcpy(&(*target_frame)[draw_y][draw_x], square_row, draw_width);
    }
}

static void draw_number(uint16_t const number, point2_t const start, uint8_t const scale)
{
    const uint8_t digit_scale = scale;      /* How much to scale each digit (scaling is done uniformly in x and y axis). */
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
            memcpy(&(*target_frame)[start.y + (scanline_y * digit_scale) + row_n][start.x], &scanline, scanline_width);
        }
    }
}

static void draw_pixel(point2_t const pos, uint8_t const color)
{
    (*target_frame)[pos.y][pos.x] = color;
}

void draw_q_line_horiz(uint16_t const row_idx, uint8_t const color)
{
    if (!draw_enabled)
    {
        return;
    }
    if (queue_idx_line_horiz >= queue_size_line_horiz)
    {
        log_error("Tried queueing a horizontal line with no space in the queue.");
        return;
    }
    line_horiz_t *const el = &queue_line_horiz[queue_idx_line_horiz++];
    el->y = row_idx;
    el->color = color;
}

void draw_q_square(point2_t const center, uint8_t const size, uint8_t const color)
{
    if (!draw_enabled)
    {
        return;
    }
    if (queue_idx_square >= queue_size_square)
    {
        log_error("Tried queueing a square with no space in the queue.");
        return;
    }
    square_t *const el = &queue_square[queue_idx_square++];
    el->center.x = center.x;
    el->center.y = center.y;
    el->size = size;
    el->color = color;
}

void draw_q_number(uint16_t const number, point2_t const start, uint8_t const scale)
{
    if (!draw_enabled)
    {
        return;
    }
    if (queue_idx_number >= queue_size_number)
    {
        log_error("Tried queueing a horizontal line with no space in the queue.");
        return;
    }
    number_t *const el = &queue_number[queue_idx_number++];
    el->start.x = start.x;
    el->start.y = start.y;
    el->number = number;
    el->scale = scale;
}

void draw_q_pixel(point2_t const pos, uint8_t const color)
{
    if (!draw_enabled)
    {
        return;
    }
    if (queue_idx_pixel >= queue_size_pixel)
    {
        log_error("Tried queueing a pixel with no space in the queue.");
        return;
    }
    pixel_t *const el = &queue_pixel[queue_idx_pixel++];
    el->pos.x = pos.x;
    el->pos.y = pos.y;
    el->color = color;
}

void draw_run(uint8_t (*const frame)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH])
{
    target_frame = frame;
    if (!draw_enabled)
    {
        return;
    }

    for (uint16_t pixel_idx = 0; pixel_idx < queue_idx_pixel; pixel_idx++)
    {
        draw_pixel(queue_pixel[pixel_idx].pos, queue_pixel[pixel_idx].color);
    }
    queue_idx_pixel = 0;

    for (uint16_t line_horiz_idx = 0; line_horiz_idx < queue_idx_line_horiz; line_horiz_idx++)
    {
        draw_line_horiz(queue_line_horiz[line_horiz_idx].y, queue_line_horiz[line_horiz_idx].color);
    }
    queue_idx_line_horiz = 0;

    for (uint16_t square_idx = 0; square_idx < queue_idx_square; square_idx++)
    {
        draw_square(queue_square[square_idx].center, queue_square[square_idx].size, queue_square[square_idx].color);
    }
    queue_idx_square = 0;

    for (uint16_t number_idx = 0; number_idx < queue_idx_number; number_idx++)
    {
        draw_number(queue_number[number_idx].number, queue_number[number_idx].start, queue_number[number_idx].scale);
    }
    queue_idx_number = 0;
}
