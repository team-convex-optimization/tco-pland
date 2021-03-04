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

void find_row_edges(uint8_t* imrow, uint8_t edges[2]) //edges is 2
{
    /* Go center to left/right and look for biggest change */
    uint8_t edge_l, edge_r, tempContrast = 0;

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
void convert_threshold(uint8_t im[TCO_SIM_HEIGHT][TCO_SIM_WIDTH]) {
    for (int i = 0; i < TCO_SIM_HEIGHT; i++) 
    {
        uint8_t thresh = find_row_threshold(im[i]);
        for (int j = 0; j < TCO_SIM_WIDTH; j++) 
        {
            im[i][j] = (im[i][j] < thresh) ? 255 : 0;
        }
    }
}

/** Edge Detection
 * @brief Return where each row is black (255) if it is an edge, else, white (0)
 * @param im a collection of grayscale pixels in format in format WIDTHxHEIGHT
 * @return void. the image is returned throught the pointer reference.
*/
void convert_scatter(uint8_t im[TCO_SIM_HEIGHT][TCO_SIM_WIDTH]){  
      return; /* TODO : implement me */
}
