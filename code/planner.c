#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#include <math.h>
#include <fcntl.h>
#include <errno.h>

#include "tco_libd.h"
#include "tco_shmem.h"
#include "tco_linalg.h"

#include "planner.h"
#include "draw.h"
#include "sort.h"
#include "misc.h"

static struct tco_shmem_data_training *shmem_training;
static sem_t *shmem_sem_training;
static struct tco_shmem_data_plan *shmem_plan;
static sem_t *shmem_sem_plan;
static uint8_t shmem_training_open = 0;
static uint8_t shmem_plan_open = 0;

static uint8_t const TRACK_CENTER_COUNT = 4;
static uint16_t track_centers[TRACK_CENTER_COUNT] = {0};
static uint8_t track_centers_push_idx = 0;

static uint16_t ray_length_last = 0;
static point2_t ray_hit = {0, 0};

/**
 * @brief Push a new track center to the track center list. This handles wrapping around.
 * @note The list works like a circular buffer.
 * @param track_center_new The track center to push.
 */
static void track_center_push(uint16_t const track_center_new)
{
    track_centers[(track_centers_push_idx + 1) % TRACK_CENTER_COUNT] = track_center_new;
    track_centers_push_idx += 1;
    track_centers_push_idx %= TRACK_CENTER_COUNT;
}

/**
 * @brief Uses all points in the track center list to find the best current center point.
 */
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

/**
 * @brief Find the track center in the provided frame.
 * @param pixels The frame where the center will be found.
 * @param bottomr_row_idx Defines the y index in the frame where the center should be found.
 * @return Track center.
 */
static point2_t track_center(uint8_t (*const pixels)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH], uint16_t const bottom_row_idx)
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
    track_center_push(track_center_new);
    point2_t const center = {track_center_compute(), bottom_row_idx};
    return center;
}

/**
 * @brief Runs for ever pixels traced by a raycast.
 * @param pixels The frame.
 * @param point Last point of the raycast.
 * @return 0 if cast should continue and -1 if cast should stop.
 */
static uint8_t raycast_callback(uint8_t (*const pixels)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH], point2_t const point)
{
    if ((*pixels)[point.y][point.x] != 255)
    {
        (*pixels)[point.y][point.x] = 120;
        ray_length_last += 1;
        ray_hit.x = point.x;
        ray_hit.y = point.y;
        return 0;
    }
    return -1;
}

/**
 * @brief Start a raycast from a @p start position in the direction of @p dir .
 * @param pixels Frame where the raycast will be shot.
 * @param start Where the raycast will begin.
 * @param dir In what direction the ray will be cast.
 * @return Length of the raycast.
 */
static uint16_t raycast(uint8_t (*const pixels)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH], point2_t const start, vec2_t const dir)
{
    draw_q_square(start, 10, 120);
    /* How much to stretch the direction vector so it touches the frame border. */
    float const edge_stretch_x = dir.x < 0 ? start.x / fabs((float)dir.x) : (TCO_FRAME_WIDTH - start.x) / fabs((float)dir.x);
    float const edge_stretch_y = dir.y < 0 ? start.y / fabs((float)dir.y) : (TCO_FRAME_HEIGHT - start.y) / fabs((float)dir.y);
    float const edge_stretch = edge_stretch_y < edge_stretch_x ? edge_stretch_y : edge_stretch_x;

    /* A direction vector which when added to start goes to the border of the frame while keeping
    angle. */
    vec2_t const dir_stretched = {dir.x * edge_stretch, dir.y * edge_stretch};
    point2_t const end = {start.x + dir_stretched.x, start.y + dir_stretched.y};

    bresenham(pixels, &raycast_callback, start, end);
    draw_q_square(ray_hit, 10, 120);

    return ray_length_last;
}

/**
 * @brief Perform radial sweep contour tracing. It will trace at most @p hops_n pixels and will
 * travel in @p cw_or_ccw (clockwise or counter-clockwise) direction. 
 * @param pixels The frame.
 * @param hops_n Max number of traced pixels.
 * @param cw_or_ccw Begin tracing clockwise or counter-clockwise.
 * @return Last point traced.
 */
static point2_t radial_sweep(uint8_t (*const pixels)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH], uint16_t hops_n, uint8_t cw_or_ccw)
{
    return (point2_t){0, 0};
}

/**
 * @brief Returns a vector in the forward direction of the track.
 * @param pixels The frame.
 * @param center Center point of the track.
 * @return A vector in the forward direction of the track. No guarantees on magnitude.
 */
static vec2_t track_orientation(uint8_t (*const pixels)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH], point2_t const center)
{
    point2_t edge_left = {center.x, center.y};
    point2_t edge_right = {center.x, center.y};
    while (edge_left.x > 0 || (*pixels)[edge_left.y][edge_left.x] == 255)
    {
        edge_left.x -= 1;
    }
    while (edge_right.x < TCO_FRAME_WIDTH || (*pixels)[edge_right.y][edge_right.x] == 255)
    {
        edge_right.x += 1;
    }

    return (vec2_t){center.x, center.y};
}

/**
 * @brief Track distances from the car to the edges of the track.
 * @param pixels The frame.
 * @param center Starting point of the raycast.
 */
static void track_distances(uint8_t (*const pixels)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH], point2_t const center)
{
    // vec2_t dir_track = track_orientation(pixels, center);
    raycast(pixels, center, (vec2_t){0, -3});
    raycast(pixels, center, (vec2_t){1, -3});
    raycast(pixels, center, (vec2_t){-1, -3});
    raycast(pixels, center, (vec2_t){2, -3});
    raycast(pixels, center, (vec2_t){-2, -3});
    raycast(pixels, center, (vec2_t){3, -3});
    raycast(pixels, center, (vec2_t){-3, -3});
    raycast(pixels, center, (vec2_t){4, -3});
    raycast(pixels, center, (vec2_t){-4, -3});
}

int plnr_init()
{
    if (shmem_map(TCO_SHMEM_NAME_TRAINING, TCO_SHMEM_SIZE_TRAINING, TCO_SHMEM_NAME_SEM_TRAINING, O_RDONLY, (void **)&shmem_training, &shmem_sem_training) != 0)
    {
        log_error("Failed to map training shmem into process memory");
        return EXIT_FAILURE;
    }
    if (shmem_map(TCO_SHMEM_NAME_PLAN, TCO_SHMEM_SIZE_PLAN, TCO_SHMEM_NAME_SEM_PLAN, O_RDWR, (void **)&shmem_plan, &shmem_sem_plan) != 0)
    {
        log_error("Failed to map planning shmem into process memory");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

int plnr_step(uint8_t (*const pixels)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH])
{
    point2_t const center = track_center(pixels, 210);
    track_distances(pixels, center);

    if (sem_wait(shmem_sem_plan) == -1)
    {
        log_error("sem_wait: %s", strerror(errno));
        return EXIT_FAILURE;
    }
    /* START: Critical section */
    shmem_plan_open = 1;
    shmem_plan->valid = 1;
    shmem_plan->target = (center.x / TCO_FRAME_WIDTH) - 1.0f;
    shmem_plan->frame_id += 1;
    /* END: Critical section */
    if (sem_post(shmem_sem_plan) == -1)
    {
        log_error("sem_post: %s", strerror(errno));
        return EXIT_FAILURE;
    }
    shmem_plan_open = 0;
    return EXIT_SUCCESS;
}

int plnr_deinit()
{
    if (shmem_plan_open)
    {
        if (sem_post(shmem_sem_plan) == -1)
        {
            log_error("sem_post: %s", strerror(errno));
            return EXIT_FAILURE;
        }
    }
    if (shmem_training_open)
    {
        if (sem_post(shmem_sem_training) == -1)
        {
            log_error("sem_post: %s", strerror(errno));
            return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}
