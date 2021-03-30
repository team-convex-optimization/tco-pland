#include <stdio.h>
#include <string.h>

#include "tco_libd.h"

#include "trajection.h"
#include "draw.h"
#include "sort.h"

static uint8_t const TRACK_CENTER_COUNT = 4;
uint16_t track_centers[TRACK_CENTER_COUNT] = {0};
uint8_t track_centers_push_idx = 0;

static void track_center_push(uint16_t track_center_new)
{
    track_centers[(track_centers_push_idx + 1) % TRACK_CENTER_COUNT] = track_center_new;
    track_centers_push_idx += 1;
    track_centers_push_idx %= TRACK_CENTER_COUNT;
}

static uint16_t track_center_compute()
{
    uint16_t track_centers_cpy[TRACK_CENTER_COUNT];
    memcpy(track_centers_cpy, track_centers, TRACK_CENTER_COUNT * sizeof(uint16_t));
    insertion_sort_integer((uint8_t *)track_centers_cpy, TRACK_CENTER_COUNT, 2, &comp_u16);

    uint16_t median;
    if (TRACK_CENTER_COUNT % 2 == 0)
    {
        /* Median of evenly long array is average of 2 central values. */
        median = (track_centers_cpy[(TRACK_CENTER_COUNT - 1) / 2] + track_centers_cpy[((TRACK_CENTER_COUNT - 1) / 2) + 1]) / 2;
    }
    else
    {
        median = track_centers_cpy[TRACK_CENTER_COUNT / 2];
    }

    return median;
}

void track_center(uint8_t (*pixels)[TCO_SIM_HEIGHT][TCO_SIM_WIDTH], uint16_t bottom_row_idx)
{
    uint16_t region_largest_size = 0;
    uint16_t region_largest_start = 0;
    uint16_t region_size = 0;
    uint16_t region_start = 0;
    for (uint16_t x = 0; x < TCO_SIM_WIDTH; x++)
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
    track_center_push(track_center_new);
    draw_square(pixels, track_center_compute(), bottom_row_idx, 10, 100);
}