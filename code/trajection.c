#include <stdio.h>
#include <string.h>
#include <math.h>

#include "tco_libd.h"

#include "trajection.h"
#include "draw.h"
#include "sort.h"
#include "lin_alg.h"
#include "misc.h"

static uint8_t const TRACK_CENTER_COUNT = 4;
static uint16_t track_centers[TRACK_CENTER_COUNT] = {0};
static uint8_t track_centers_push_idx = 0;

static uint16_t ray_length_last = 0;

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

void track_center(uint8_t (*pixels)[TCO_SIM_HEIGHT][TCO_SIM_WIDTH], uint16_t starttom_row_idx)
{
    uint16_t region_largest_size = 0;
    uint16_t region_largest_start = 0;
    uint16_t region_size = 0;
    uint16_t region_start = 0;
    for (uint16_t x = 0; x < TCO_SIM_WIDTH; x++)
    {
        if ((*pixels)[starttom_row_idx][x] == 255)
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
    draw_square(pixels, (point2_t){track_center_compute(), starttom_row_idx}, 10, 100);
}

static uint8_t shoot_ray_callback(uint8_t (*const pixels)[TCO_SIM_HEIGHT][TCO_SIM_WIDTH], point2_t point)
{
    if ((*pixels)[point.y][point.x] == 0)
    {
        ray_length_last += 1;
    }
    else
    {
        return -1;
    }
    draw_square(pixels, point, 1, 120);
    return 0;
}

static point2_t shoot_ray(uint8_t (*pixels)[TCO_SIM_HEIGHT][TCO_SIM_WIDTH], point2_t const start, vec2_t const dir)
{
    /* How much to stretch the direction vector so it touches the frame border. */
    float const edge_stretch_x = dir.x < 0 ? start.x / fabs((float)dir.x) : (TCO_SIM_WIDTH - start.x) / fabs((float)dir.x);
    log_debug("edge stretch x: %f", edge_stretch_x);
    float const edge_stretch_y = dir.y < 0 ? start.y / fabs((float)dir.y) : (TCO_SIM_HEIGHT - start.y) / fabs((float)dir.y);
    log_debug("edge stretch y: %f", edge_stretch_y);

    float const edge_stretch = edge_stretch_y < edge_stretch_x ? edge_stretch_y : edge_stretch_x;
    log_debug("selected edge stretch: %f", edge_stretch);

    /* A direction vector which when added to start goes to the border of the frame while keeping angle. */
    vec2_t const dir_stretched = {dir.x * edge_stretch, dir.y * edge_stretch};
    log_debug("dir stretched %i %i, startx: %u %u, +: %u %u", dir_stretched.x, dir_stretched.y, start.x, start.y, start.x + dir_stretched.x, start.y + dir_stretched.y);
    point2_t const end = {start.x + dir_stretched.x, start.y + dir_stretched.y};

    log_debug("end %u %u", end.x, end.y);
    draw_square(pixels, end, 10, 120);

    bresenham(pixels, &shoot_ray_callback, start, end);

    return end;
}

void track_distances(uint8_t (*pixels)[TCO_SIM_HEIGHT][TCO_SIM_WIDTH], uint16_t starttom_row_idx)
{
    point2_t const start = {200, starttom_row_idx};
    vec2_t const dir = {40, -20};
    point2_t const hit = shoot_ray(pixels, start, dir);

    draw_square(pixels, start, 10, 120);
    draw_square(pixels, (point2_t){start.x + dir.x, start.y + dir.y}, 10, 120);
    log_debug("drawing end: %u %u", hit.x, hit.y);
    draw_square(pixels, hit, 10, 120);
}
