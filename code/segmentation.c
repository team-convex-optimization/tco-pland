#include <stdlib.h>
#include "segmentation.h"
#include "tco_libd.h"

void segment(uint8_t (*pixels)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH])
{
    uint8_t const delta_threshold = 110;
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
