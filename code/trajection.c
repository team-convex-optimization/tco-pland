#include "trajection.h"

uint8_t find_row_threshold(uint8_t* imrow)
{
    /* Find the greatest change in pixel values. This is likely edge */
    uint8_t gradient = 0;
    for (int i = 0; i < TCO_SIM_WIDTH - 1; i++) 
    {
        uint8_t diff = (imrow[i] > imrow[i+1]) ? (imrow[i]-imrow[i+1]): (imrow[i+1]-imrow[i]);
        if (diff > gradient) 
        {
            gradient = diff;
        } 
    }
    return gradient;
}

void find_row_edges(uint8_t* imrow, uint16_t* edges) //edges is 2
{
    /* Go center to left/right and look for biggest change */
    uint16_t edge_l, edge_r, tempContrast = 0;

    /* Find Left edge */
    for (int i = IMAGE_CENTER; i > 1; i--) 
    {
        uint8_t contrast = (imrow[i] > imrow[i-1]) ? (imrow[i]-imrow[i-1]): (imrow[i-1]-imrow[i]);
        if (contrast > tempContrast)
        {
            tempContrast = contrast;
            edge_l = i;
        }
    }

    /* Find right Edge */
    tempContrast = 0; 
    for (int i = IMAGE_CENTER; i < TCO_SIM_WIDTH; i++) 
    {
        uint8_t contrast = (imrow[i] > imrow[i+1]) ? (imrow[i]-imrow[i+1]): (imrow[i+1]-imrow[i]);
        if (contrast > tempContrast)
        {
            tempContrast = contrast;
            edge_r = i;
        }
    }

    /* Passed by ref */
    edges[0] = edge_l;
    edges[1] = edge_r;
}

/** Dynamic Thresholding
 * @brief Return where each row is black (255) if it is an edge, else, white (0)
 * @param im a collection of grayscale pixels in format in format WIDTHxHEIGHT
 * @return void. the image is returned throught the pointer reference.
*/
void convert_threshold(uint8_t* im) {
    for (int i = 0; i < TCO_SIM_HEIGHT; i++) 
    {
        uint8_t *row = &im[i*TCO_SIM_WIDTH];
        uint8_t thresh = find_row_threshold(row);
        for (int j = 0; j < TCO_SIM_WIDTH; j++) 
        {
            row[j] = (row[j] < thresh) ? 255 : 0;
        }
    }
}

/** Edge Detection
 * @brief Return where each row is black (255) if it is an edge, else, white (0)
 * @param im a collection of grayscale pixels in format in format WIDTHxHEIGHT
 * @return void. the image is returned throught the pointer reference.
*/
void convert_scatter(uint8_t* im){  
    for (int i = 0; i < TCO_SIM_HEIGHT; i++) 
    {
        uint8_t *row = &im[i*TCO_SIM_WIDTH];
        uint16_t edges[2]; 
        find_row_edges(row, (uint16_t*)&edges);
        /* Paint the edges white */
        for (uint16_t j = 0; j < TCO_SIM_WIDTH; j++) 
        {
            row[j] = (j == edges[0] || j == edges[1]) ? 255 : 0;
        }
    }
}

/** K nearest gradient track-center search
 * @brief todo
 * @param im is a collection of pixels with values 255 OR 0. That is, it is B&W.
 * @param k is the number of neighbors to look at when finding slope. must be odd.
 * @return todo
*/
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
