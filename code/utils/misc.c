#include <stdlib.h>

#include <math.h>

#include "misc.h"
#include "buf_circ.h"
#include "draw.h"

uint16_t bresenham(uint8_t (*pixels)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH],
                   callback_func_t const pixel_action,
                   point2_t const start,
                   point2_t const end)
{
    if (start.x >= TCO_FRAME_WIDTH || end.x >= TCO_FRAME_WIDTH || start.y >= TCO_FRAME_HEIGHT || end.y >= TCO_FRAME_HEIGHT)
    {
        return 0;
    }

    int16_t dx = abs(end.x - start.x), sx = start.x < end.x ? 1 : -1;
    int16_t dy = -abs(end.y - start.y), sy = start.y < end.y ? 1 : -1;
    int16_t err = dx + dy, e2; /* error value e_xy */

    uint16_t x = start.x;
    uint16_t y = start.y;

    uint16_t length = 0;

    for (;;)
    {
        if (pixel_action != NULL && pixel_action(pixels, (point2_t){x, y}) != 0)
        {
            break;
        }
        if (x == end.x && y == end.y)
        {
            break;
        }
        length++;
        e2 = 2 * err;
        if (e2 >= dy)
        {
            err += dy;
            x += sx;
        } /* e_xy+e_x > 0 */
        if (e2 <= dx)
        {
            err += dx;
            y += sy;
        } /* e_xy+e_y < 0 */
    }
    return length;
}

point2_t radial_sweep(
    uint8_t (*const pixels)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH],
    vec2_t *const circ_data,
    uint16_t const circ_data_len,
    point2_t const start,
    uint16_t const contour_length,
    uint8_t const cw_or_ccw,
    float const radial_start,
    float const radial_len_max,
    uint8_t *const status)
{
    static uint16_t const trace_margin = 10;

    uint16_t const circ_size = circ_data_len;
    uint16_t const radial_count_max = radial_len_max * circ_size;
    uint16_t const quarter_size = circ_size / 4;
    uint16_t const circ_idx_90deg = quarter_size + quarter_size + quarter_size;
    uint16_t const circ_idx_270deg = quarter_size;

    buf_circ_t circ_buf = {(void *)circ_data, circ_size, circ_size - 1, sizeof(vec2_t)};
    uint16_t circ_idx = radial_start * circ_size;
    draw_q_square((point2_t){start.x + circ_data[circ_idx].x, start.y + circ_data[circ_idx].y}, 4, 150);
    point2_t trace_last = start;

    for (uint16_t contour_length_now = 0; contour_length_now < contour_length; contour_length_now++)
    {
        for (uint16_t swept_pts = 0; swept_pts < circ_size - 1; swept_pts++)
        {
            vec2_t const circ_vec = *((vec2_t *)buf_circ_get(&circ_buf, circ_idx));
            point2_t const trace_target = {trace_last.x + circ_vec.x, trace_last.y + circ_vec.y};

            /* Outside bounds. Also stop if the traced radial length is too long. */
            if (trace_target.y >= TCO_FRAME_HEIGHT - trace_margin ||
                trace_last.x >= TCO_FRAME_WIDTH - trace_margin ||
                trace_last.x <= trace_margin ||
                trace_target.y <= trace_margin ||
                swept_pts >= radial_count_max)
            {
                if (swept_pts >= radial_count_max)
                {
                    *status = 3;
                }
                else
                {
                    *status = 2;
                }
                return trace_last;
            }
            draw_q_pixel(trace_target, 120);

            /* End current sweep when a white point is found and move onto the next one. */
            if ((*pixels)[trace_target.y][trace_target.x] > 0)
            {
                trace_last = trace_target;
                /* Sweep from a normal to the current point. */
                if (cw_or_ccw)
                {
                    circ_idx = circ_idx + circ_idx_90deg;
                }
                else
                {
                    circ_idx = circ_idx + circ_idx_270deg;
                }
                break;
            }

            /* Go to next circle point. */
            if (cw_or_ccw)
            {
                circ_idx += 1;
            }
            else
            {
                circ_idx += circ_size - 1; /* "buf_circ_get" will wrap-around automatically. */
            }

            /* If swept through whole circle (excluding starting pos) without a trace, just end
            prematurely.  */
            if (swept_pts >= circ_size - 1)
            {
                *status = 1;
                return trace_last;
            }
        }
    }
    *status = 0;
    return trace_last;
}

uint16_t raycast(uint8_t (*const pixels)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH],
                 point2_t const start,
                 vec2_t const dir,
                 callback_func_t const callback)
{
    /* How much to stretch the direction vector so it touches the frame border. */
    float const edge_stretch_x = dir.x < 0 ? start.x / fabs((float)dir.x) : (TCO_FRAME_WIDTH - 1 - start.x) / fabs((float)dir.x);
    float const edge_stretch_y = dir.y < 0 ? start.y / fabs((float)dir.y) : (TCO_FRAME_HEIGHT - 1 - start.y) / fabs((float)dir.y);
    float const edge_stretch = edge_stretch_y < edge_stretch_x ? edge_stretch_y : edge_stretch_x;

    /* A direction vector which when added to start goes to the border of the frame while keeping
    angle. */
    vec2_t const dir_stretched = {dir.x * edge_stretch, dir.y * edge_stretch};
    point2_t const end = {start.x + dir_stretched.x, start.y + dir_stretched.y};

    return bresenham(pixels, callback, (point2_t){start.x, start.y}, end);
}

uint8_t cb_draw_light_stop_white(uint8_t (*const pixels)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH], point2_t const point)
{
    if (!draw_enabled)
    {
        return 0;
    }
    if ((*pixels)[point.y][point.x] != 255)
    {
        draw_q_pixel(point, 120);
        return 0;
    }
    return -1;
}

uint8_t cb_draw_light_stop_no(uint8_t (*const pixels)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH], point2_t const point)
{
    if (!draw_enabled)
    {
        return 0;
    }
    draw_q_pixel(point, 120);
    return 0;
}

uint8_t cb_draw_perm_stop_no(uint8_t (*const pixels)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH], point2_t const point)
{
    if (!draw_enabled)
    {
        return 0;
    }
    (*pixels)[point.y][point.x] = 255;
    return 0;
}

uint8_t cb_draw_no_stop_white(uint8_t (*const pixels)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH], point2_t const point)
{
    if (!draw_enabled)
    {
        return 0;
    }
    if ((*pixels)[point.y][point.x] != 255)
    {
        return 0;
    }
    return -1;
}

point2_t track_center(uint8_t (*const pixels)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH], uint16_t const bottom_row_idx)
{
    uint16_t region_largest_size = 0;
    uint16_t region_largest_start = 0;
    uint16_t region_size = 0;
    uint16_t region_start = 0;
    for (uint16_t x = 0; x < TCO_FRAME_WIDTH; x++)
    {
        if ((*pixels)[bottom_row_idx][x] == 255)
        {
            region_size += 1;
        }
        else
        {
            if (region_size > region_largest_size)
            {
                region_largest_start = region_start;
                region_largest_size = region_size;
            }
            region_start = x;
            region_size = 0;
        }
    }
    if (region_size > region_largest_size)
    {
        region_largest_start = region_start;
        region_largest_size = region_size;
    }
    uint16_t track_center_new = region_largest_start + (region_largest_size / 2);
    point2_t const center = {track_center_new, bottom_row_idx};

    /* Center should never be at x=0. */
    if (center.x == 0)
    {
        return (point2_t){TCO_FRAME_WIDTH / 2, center.y};
    }
    else
    {
        return center;
    }
}

point2_t track_center_black(uint8_t (*const pixels)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH], uint16_t const bottom_row_idx)
{
    point2_t const center = track_center(pixels, bottom_row_idx);
    point2_t center_black = center;
    while (center_black.y - 1 > 0 && (*pixels)[center_black.y][center_black.x] != 0)
    {
        center_black.y--;
    }
    return center_black;
}

uint8_t check_bounds_inside(int16_t x, int16_t y)
{
    if (x >= 0 && x < TCO_FRAME_WIDTH && y >= 0 && y < TCO_FRAME_HEIGHT)
    {
        return 0;
    }
    else
    {
        return -1;
    }
}
