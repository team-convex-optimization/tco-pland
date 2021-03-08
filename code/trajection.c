#include "trajection.h"


void k_nearest_neighbor_gradient_center(uint8_t* im, const uint8_t k) {
    uint16_t edges[TCO_SIM_HEIGHT][2]; /* This represents the start and end of the track at any row */

      // if left edge is black, start search from right edge and vice versa. We work bottom up!
    for (int i = 0; i < TCO_SIM_HEIGHT; i++) 
    {
        uint8_t *row = &im[i*TCO_SIM_WIDTH];
        uint16_t index = 0;
        while (row[index++] != 0 && index < TCO_SIM_WIDTH) {}
        edges[i][0] = index;
        index = TCO_SIM_WIDTH-1;
        while (row[index--] != 0 && index > 0) {}
        edges[i][1] = index;
    }
        
    /* Test code to draw the edges */
    for (int i = 0; i < TCO_SIM_HEIGHT; i++) 
    {
        uint8_t *row = &im[i*TCO_SIM_WIDTH];
        for (int j = 0; j < TCO_SIM_WIDTH; j++)
        {
            if (j == edges[i][0] || j == edges[i][1])
            {
                row[j] = 255;
            }
            else
            {
                row[j] = 0;
            }
        }
    }

    // Find center using dynamic gradients along edges!! This will allow horizontal lines as track curves. 
    // Take seed as bottom 30 rows which are quite reliable. Then if gradient has to be larger than this you 
    // have to add to manipulate previous's row data.
}
