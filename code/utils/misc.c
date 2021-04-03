#include <stdlib.h>

#include <math.h>

#include "misc.h"
#include "buf_circ.h"
#include "draw.h"

void bresenham(uint8_t (*pixels)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH],
               uint8_t (*pixel_action)(uint8_t (*const)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH], point2_t const),
               point2_t const start,
               point2_t const end)
{
    if (start.x >= TCO_FRAME_WIDTH || end.x >= TCO_FRAME_WIDTH || start.y >= TCO_FRAME_HEIGHT || end.y >= TCO_FRAME_HEIGHT)
    {
        return;
    }

    int16_t dx = abs(end.x - start.x), sx = start.x < end.x ? 1 : -1;
    int16_t dy = -abs(end.y - start.y), sy = start.y < end.y ? 1 : -1;
    int16_t err = dx + dy, e2; /* error value e_xy */

    uint16_t x = start.x;
    uint16_t y = start.y;

    for (;;)
    {
        if (pixel_action(pixels, (point2_t){x, y}) != 0)
        {
            break;
        }
        if (x == end.x && y == end.y)
        {
            break;
        }
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
}

point2_t radial_sweep(uint8_t (*const pixels)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH], point2_t const start, uint16_t const contour_length, uint8_t const cw_or_ccw)
{
    static uint16_t const trace_margin = 10;
    /* Generated with "tco_circle_vector_gen" for a radius 6 circle. */
    /* Up -> Q1 -> Right -> Q4 -> Down -> Q3 -> Left -> Q2 -> (wrap-around to Up) */
    static vec2_t const circ_data[] = {
        {0, -6},
        {1, -6},
        {2, -6},
        {3, -5},
        {4, -5},
        {5, -4},
        {5, -3},
        {6, -2},
        {6, -1},
        {6, 0},
        {6, 1},
        {6, 2},
        {5, 3},
        {5, 4},
        {4, 5},
        {3, 5},
        {2, 6},
        {1, 6},
        {0, 6},
        {-1, 6},
        {-2, 6},
        {-3, 5},
        {-4, 5},
        {-5, 4},
        {-5, 3},
        {-6, 2},
        {-6, 1},
        {-6, 0},
        {-6, -1},
        {-6, -2},
        {-5, -3},
        {-5, -4},
        {-4, -5},
        {-3, -5},
        {-2, -6},
        {-1, -6},
    };
    uint16_t const circ_size = sizeof(circ_data) / sizeof(vec2_t);
    uint16_t const quarter_size = circ_size / 4;
    uint16_t const circ_idx_90deg = quarter_size + quarter_size + quarter_size;
    uint16_t const circ_idx_270deg = quarter_size;

    buf_circ_t circ_buf = {(void *)circ_data, circ_size, circ_size - 1, sizeof(vec2_t)};
    uint16_t circ_idx = 0;
    point2_t trace_last = start;
    if (cw_or_ccw)
    {
        circ_idx = circ_idx_90deg;
    }
    else
    {
        circ_idx = circ_idx_270deg;
    }
    for (uint16_t contour_length_now = 0; contour_length_now < contour_length; contour_length_now++)
    {
        for (uint16_t swept_pts = 0; swept_pts < circ_size - 1; swept_pts++)
        {
            vec2_t const circ_vec = *((vec2_t *)buf_circ_get(&circ_buf, circ_idx));
            point2_t const trace_target = {trace_last.x + circ_vec.x, trace_last.y + circ_vec.y};

            /* Outside bounds */
            if (trace_target.y >= TCO_FRAME_HEIGHT - trace_margin ||
                trace_last.x >= TCO_FRAME_WIDTH - trace_margin ||
                trace_last.x <= trace_margin ||
                trace_target.y <= trace_margin)
            {
                return trace_last;
            }
            draw_q_pixel(trace_target, 120);

            /* End current sweep when a white point is found and move onto the next one. */
            if ((*pixels)[trace_target.y][trace_target.x] == 255)
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
                return trace_last;
            }
        }
    }
    return trace_last;
}

void raycast(uint8_t (*const pixels)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH],
             point2_t const start,
             vec2_t const dir,
             uint8_t (*const callback)(uint8_t (*const)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH], point2_t const))
{
    /* How much to stretch the direction vector so it touches the frame border. */
    float const edge_stretch_x = dir.x < 0 ? start.x / fabs((float)dir.x) : (TCO_FRAME_WIDTH - 1 - start.x) / fabs((float)dir.x);
    float const edge_stretch_y = dir.y < 0 ? start.y / fabs((float)dir.y) : (TCO_FRAME_HEIGHT - 1 - start.y) / fabs((float)dir.y);
    float const edge_stretch = edge_stretch_y < edge_stretch_x ? edge_stretch_y : edge_stretch_x;

    /* A direction vector which when added to start goes to the border of the frame while keeping
    angle. */
    vec2_t const dir_stretched = {dir.x * edge_stretch, dir.y * edge_stretch};
    point2_t const end = {start.x + dir_stretched.x, start.y + dir_stretched.y};

    bresenham(pixels, callback, (point2_t){start.x, start.y}, end);
}
