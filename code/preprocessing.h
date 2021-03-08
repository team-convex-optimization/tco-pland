#ifndef _PREPROCESSING_H_
#define _PREPROCESSING_H_

#include "tco_shmem.h"
#include "tco_libd.h"

/**
 * This code contains computations for preprocessing an image before 
 * finding any trajections. Sharpening Edges, Canny processing etc are
 * all examples of ways 'features' can be enhanced from raw pixel data.
 * Here we want to find the edges of the track. 
 */

/*****************************************************
 *************** Important definitions ***************
 ****************************************************/
typedef uint8_t image[TCO_SIM_HEIGHT][TCO_SIM_WIDTH];
#define IMAGE_CENTER TCO_SIM_WIDTH/2

/*****************************************************
 **************** Pixel Row Operations ***************
 ****************************************************/

/** Dynamic Thresholding
 * @brief Find track edges by finding greatest change in pixel value
 * @param imrow a single row of grayscale pixels of size TCO_SIM_WIDTH
 * @return the value of the grayscale pixel with highest contrast to nearest neighbor.
*/
uint8_t find_row_threshold(uint8_t* imrow);


/** Edge Detection
 * @brief Find track edges by working from the center of the track
 * @param imrow a single row of grayscale pixels of size TCO_SIM_WIDTH
 * @param edges should be a reference to a 2x1 byte array of where the result of edge_l and edge_r should be placed. 
 *              Each edge is the number of pixels from the left where the edge is on the row
*/
void find_row_edges(uint8_t* im, uint16_t *edges);


/*****************************************************
 *************** Image Transformations ***************
 ****************************************************/

/** Dynamic Thresholding
 * @brief Return where each row is black (255) if it is an edge, else, white (0)
 * @param im a collection of grayscale pixels in format in format WIDTHxHEIGHT
 * @return void. the image is returned throught the pointer reference.
*/
void convert_threshold(uint8_t* im);

/** Edge Detection
 * @brief Return where each row is black (255) if it is an edge, else, white (0)
 * @param im a collection of grayscale pixels in format in format WIDTHxHEIGHT
 * @return void. the image is returned throught the pointer reference.
*/
void convert_scatter(uint8_t* im);

#endif /* PREPROCESSING */