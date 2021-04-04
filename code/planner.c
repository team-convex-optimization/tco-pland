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
#include "buf_circ.h"

static struct tco_shmem_data_training *shmem_training;
static sem_t *shmem_sem_training;
static struct tco_shmem_data_plan *shmem_plan;
static sem_t *shmem_sem_plan;
static uint8_t shmem_training_open = 0;
static uint8_t shmem_plan_open = 0;
static vec2_t track_dir_last = {0, -1};
static point2_t track_center_black_last = {TCO_FRAME_WIDTH / 2, 210 - 10};

static uint8_t const track_center_count = 4;
static uint16_t track_centers_data[track_center_count] = {0};
static buf_circ_t track_centers = {track_centers_data, track_center_count, track_center_count - 1, sizeof(uint16_t)};

/* All these matrices have been generated using the 'tco_matrix_gen' utility. */
/* Rotation matrix: [[cos(20 deg) -sin(20 deg)], [sin(20 deg) cos(20 deg)]] */
static float const rot_cw20_mat_data[4] = {0.939692620786f, -0.342020143326f, 0.342020143326f, 0.939692620786f};
static matf_t const rot_cw20_matrix = {(float *)rot_cw20_mat_data, 2, 2};

/* Rotation matrix: [[cos(-20 deg) -sin(-20 deg)], [sin(-20 deg) cos(-20 deg)]] */
static float const rot_ccw20_mat_data[4] = {0.939692620786f, 0.342020143326f, -0.342020143326f, 0.939692620786f};
static matf_t const rot_ccw20_matrix = {(float *)rot_ccw20_mat_data, 2, 2};

/* Rotation matrix: [[cos(10 deg) -sin(10 deg)], [sin(10 deg) cos(10 deg)]] */
static float const rot_cw10_mat_data[4] = {0.984807753012f, -0.173648177667f, 0.173648177667f, 0.984807753012f};
static matf_t const rot_cw10_matrix = {(float *)rot_cw10_mat_data, 2, 2};

/* Rotation matrix: [[cos(-10 deg) -sin(-10 deg)], [sin(-10 deg) cos(-10 deg)]] */
static float const rot_ccw10_mat_data[4] = {0.984807753012f, 0.173648177667f, -0.173648177667f, 0.984807753012f};
static matf_t const rot_ccw10_matrix = {(float *)rot_ccw10_mat_data, 2, 2};

/**
 * @brief Given a list of uint16_t values, finds the median and returns it.
 * @param list List of values to find median inside.
 * @param length Number of elements in the @p list .
 * @return Median of all values in the @p list .
 */
static uint16_t listu16_median(uint16_t *const list, uint16_t const length)
{
    uint16_t list_cpy[length];
    memcpy(list_cpy, list, length * sizeof(uint16_t));
    insertion_sort_integer((uint8_t *)list_cpy, length, sizeof(uint16_t), &comp_u16);

    uint16_t median;
    if (track_center_count % 2 == 0)
    {
        /* Median of evenly long array is average of 2 central values. */
        median = (list_cpy[(length - 1) / 2] + list_cpy[((length - 1) / 2) + 1]) / 2;
    }
    else
    {
        median = list_cpy[length / 2];
    }
    return median;
}

/**
 * @brief Find the track center in the provided frame.
 * @param pixels The frame where the center will be found. It needs to be a segmented frame.
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
    buf_circ_add(&track_centers, &track_center_new);
    point2_t const center = {listu16_median(track_centers_data, track_center_count), bottom_row_idx};

    /* Center should never be at x=0. */
    if (center.x == 0)
    {
        return (point2_t){TCO_FRAME_WIDTH / 2, center.y};
    }
    else
    {
        return center;
    }
}

/**
 * @brief Runs for ever pixels traced by a raycast.
 * @param pixels A segmented frame.
 * @param point Last point of the raycast.
 * @return 0 if cast should continue and -1 if cast should stop.
 */
static uint8_t raycast_callback(uint8_t (*const pixels)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH], point2_t const point)
{
    if ((*pixels)[point.y][point.x] != 255)
    {
        draw_q_pixel(point, 120);
        return 0;
    }
    return -1;
}

/**
 * @brief Runs for ever pixels traced by a raycast. It sets all points on the raycast to white on the image permanently.
 * @param pixels A segmented frame.
 * @param point Last point of the raycast.
 * @return 0 if cast should continue and -1 if cast should stop.
 */
static uint8_t raycast_draw_callback(uint8_t (*const pixels)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH], point2_t const point)
{
    if ((*pixels)[point.y][point.x] != 255)
    {
        (*pixels)[point.y][point.x] = 255;
        if (point.x - 2 > 0)
        {
            (*pixels)[point.y][point.x - 2] = 255;
        }
        if (point.x - 1 > 0)
        {
            (*pixels)[point.y][point.x - 1] = 255;
        }
        if (point.x + 1 < TCO_FRAME_WIDTH)
        {
            (*pixels)[point.y][point.x + 1] = 255;
        }
        if (point.x + 2 < TCO_FRAME_WIDTH)
        {
            (*pixels)[point.y][point.x + 2] = 255;
        }
        return 0;
    }
    return -1;
}

/**
 * @brief Find an edge of the track.
 * @param pixels A segmented frame where to search.
 * @param center_black Where to start searching. This must be the track center and must lie on top
 * of a black pixel.
 * @param left_or_right If 1 then left edge is returned, if 0 then right edge is returned.
 * @return Edge of the track.
 */
static point2_t track_edge(uint8_t (*const pixels)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH], point2_t const center, uint8_t const left_or_right)
{
    int8_t search_delta = left_or_right ? -1 : 1;
    uint16_t edge_x = center.x;
    while (edge_x > 0 && edge_x < TCO_FRAME_WIDTH)
    {
        if ((*pixels)[center.y][edge_x] == 255)
        {
            break;
        }
        edge_x += search_delta;
    }
    return (point2_t){edge_x, center.y};
}

/**
 * @brief Given a point where the edge starts, this returns the direction that edge is pointed in.
 * @param pixels A segmented frame.
 * @param left_or_right Select if traced edge is the left or right edge.
 * @param edge_start Where the edge starts.
 * @return Forward direction of the edge.
 */
static vec2_t track_edge_dir(uint8_t (*const pixels)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH], uint8_t const left_or_right, point2_t const edge_start)
{
    point2_t edge_traced;
    uint8_t edge_missing = 1;

    if ((*pixels)[edge_start.y][edge_start.x] == 255)
    {
        uint8_t status_sweep;
        if (left_or_right)
        {
            edge_traced = radial_sweep(pixels, edge_start, 20, 0, 1.0f, &status_sweep);
        }
        else
        {
            edge_traced = radial_sweep(pixels, edge_start, 20, 1, 1.0f, &status_sweep);
        }

        if (edge_start.x != edge_traced.x && edge_start.y != edge_traced.y)
        {
            edge_missing = 0;
        }

        vec2_t const edge_dir = {edge_traced.x - edge_start.x, edge_traced.y - edge_start.y};
        return edge_dir;
    }
    return (vec2_t){0, 0}; /* Missing edge. */
}

/**
 * @brief Returns a vector in the forward direction of the track.
 * @param pixels The frame.
 * @param edge_left Left edge of the track.
 * @param edge_right Right edge of the track.
 * @return A vector in the forward direction of the track. No guarantees on magnitude.
 */
static vec2_t track_orientation(uint8_t (*const pixels)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH], point2_t const edge_left, point2_t const edge_right)
{
    static vec2_t dir_last;

    /* If an edge is missing, the corresponding vector will be {0,0}. */
    vec2_t edge_left_dir = track_edge_dir(pixels, 1, edge_left);
    vec2_t edge_right_dir = track_edge_dir(pixels, 0, edge_right);

    uint8_t const edge_left_missing = edge_left_dir.x == 0 && edge_left_dir.y == 0 ? 1 : 0;
    uint8_t const edge_right_missing = edge_right_dir.x == 0 && edge_right_dir.y == 0 ? 1 : 0;

    /* Defaulting to straight ahead. */
    vec2_t track_dir_estimated = {0, -40};
    if (!edge_left_missing && !edge_right_missing)
    {
        /* No edges missing so average of the 2 will be a good estimation of track direction. */
        float const edge_left_inv_100length = 100.0f / sqrt(edge_left_dir.x * edge_left_dir.x + edge_left_dir.y * edge_left_dir.y);
        float const edge_right_inv_100length = 100.0f / sqrt(edge_right_dir.x * edge_right_dir.x + edge_right_dir.y * edge_right_dir.y);

        track_dir_estimated.x = ((edge_left_dir.x * edge_left_inv_100length) + (edge_right_dir.x * edge_right_inv_100length)) / 2;
        track_dir_estimated.y = ((edge_left_dir.y * edge_left_inv_100length) + (edge_right_dir.y * edge_right_inv_100length)) / 2;

        draw_q_square((point2_t){edge_left.x + edge_left_dir.x, edge_left.y + edge_left_dir.y}, 10, 120);
        draw_q_square((point2_t){edge_right.x + edge_right_dir.x, edge_right.y + edge_right_dir.y}, 10, 120);
    }
    else if (edge_right_missing && !edge_left_missing)
    {
        /* Right edge missing so left is used directly. */
        track_dir_estimated = edge_left_dir;
    }
    else if (edge_left_missing && !edge_right_missing)
    {
        /* Left edge missing so right is used directly. */
        track_dir_estimated = edge_right_dir;
    }
    else
    {
        track_dir_estimated = dir_last;
    }
    dir_last = track_dir_estimated;

    /* TODO: Apply direction heuristics. */
    return track_dir_estimated;
}

/**
 * @brief Track distances from the car to the furthest point ahead on the track if measured parallel
 * to the track.
 * @param pixels A segmented frame.
 * @param center_black Starting point of the raycast. It must sit on top of a black pixel.
 * @param dir_track Forward direction of the track.
 * @return Distance ahead of the car to the edge of the track (mesures paralell to the track along
 * the centerline).
 */
static uint16_t track_distance(uint8_t (*const pixels)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH], point2_t const center_black, vec2_t const dir_track)
{
    float const dir_track_inv_length = 1.0f / sqrt(dir_track.x * dir_track.x + dir_track.y * dir_track.y);

    /* Normalize track direction to a known length. */
    float const track_dir_short_data[2] = {dir_track.x * dir_track_inv_length * 40.0f, dir_track.y * dir_track_inv_length * 40.0f};
    matf_t const track_dir_short_mat = {(float *)track_dir_short_data, 2, 1};

    float vec_rot_cw20_data[2];
    float vec_rot_ccw20_data[2];
    float vec_rot_cw10_data[2];
    float vec_rot_ccw10_data[2];
    matf_t vec_rot_cw20 = {(float *)vec_rot_cw20_data, 2, 1};
    matf_t vec_rot_ccw20 = {(float *)vec_rot_ccw20_data, 2, 1};
    matf_t vec_rot_cw10 = {(float *)vec_rot_cw10_data, 2, 1};
    matf_t vec_rot_ccw10 = {(float *)vec_rot_ccw10_data, 2, 1};

    /* Create two vectors, one rotated clockwise and other counter-clockwise relative to track
    direction. */
    matf_mul_matf(&rot_cw20_matrix, &track_dir_short_mat, &vec_rot_cw20);
    matf_mul_matf(&rot_ccw20_matrix, &track_dir_short_mat, &vec_rot_ccw20);
    matf_mul_matf(&rot_cw10_matrix, &track_dir_short_mat, &vec_rot_cw10);
    matf_mul_matf(&rot_ccw10_matrix, &track_dir_short_mat, &vec_rot_ccw10);

    vec2_t const dir0 = {track_dir_short_data[0], track_dir_short_data[1]};
    vec2_t const dir1 = {vec_rot_cw20_data[0], vec_rot_cw20_data[1]};
    vec2_t const dir2 = {vec_rot_ccw20_data[0], vec_rot_ccw20_data[1]};
    vec2_t const dir3 = {vec_rot_cw10_data[0], vec_rot_cw10_data[1]};
    vec2_t const dir4 = {vec_rot_ccw10_data[0], vec_rot_ccw10_data[1]};

    draw_q_square((point2_t){dir0.x + center_black.x, dir0.y + center_black.y}, 10, 100);
    draw_q_square((point2_t){dir1.x + center_black.x, dir1.y + center_black.y}, 10, 150);
    draw_q_square((point2_t){dir2.x + center_black.x, dir2.y + center_black.y}, 10, 150);
    draw_q_square((point2_t){dir3.x + center_black.x, dir3.y + center_black.y}, 10, 150);
    draw_q_square((point2_t){dir4.x + center_black.x, dir4.y + center_black.y}, 10, 150);

    uint16_t distance_total = 0;
    distance_total += raycast(pixels, center_black, dir0, &raycast_callback);
    distance_total += raycast(pixels, center_black, dir1, &raycast_callback);
    distance_total += raycast(pixels, center_black, dir2, &raycast_callback);
    distance_total += raycast(pixels, center_black, dir3, &raycast_callback);
    distance_total += raycast(pixels, center_black, dir4, &raycast_callback);
    return distance_total / 5;
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
    point2_t const center_black = (point2_t){center.x, center.y - 10};
    track_center_black_last = center_black;

    point2_t const edge_left = track_edge(pixels, center_black, 1);
    point2_t const edge_right = track_edge(pixels, center_black, 0);
    vec2_t const dir_track = track_orientation(pixels, edge_left, edge_right);
    track_dir_last = dir_track;

    // uint16_t distance_ahead = track_distance(pixels, center_black, dir_track);

    draw_q_square((point2_t){dir_track.x + center_black.x, dir_track.y + center_black.y}, 10, 150);
    raycast(pixels, center_black, dir_track, &raycast_callback);
    draw_q_square(center_black, 10, 150);

    if (sem_wait(shmem_sem_plan) == -1)
    {
        log_error("sem_wait: %s", strerror(errno));
        return EXIT_FAILURE;
    }
    /* START: Critical section */
    shmem_plan_open = 1;
    shmem_plan->valid = 1;
    shmem_plan->target = 0.0f;
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
