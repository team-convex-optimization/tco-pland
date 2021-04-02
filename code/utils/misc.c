#include <stdlib.h>

#include "misc.h"

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