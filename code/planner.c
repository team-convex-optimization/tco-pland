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

static uint16_t const track_width = 300; /* Pixels */

/* Generated with "tco_circle_vector_gen" for a radius 6 circle. */
/* Up -> Q1 -> Right -> Q4 -> Down -> Q3 -> Left -> Q2 -> (wrap-around to Up) */
static vec2_t const circ_data[] = {
    {0, -6},
    {1, -6},
    {2, -6},
    {3, -5},
    {4, -5},
    {5, -4},
    {5, -3},
    {6, -2},
    {6, -1},
    {6, 0},
    {6, 1},
    {6, 2},
    {5, 3},
    {5, 4},
    {4, 5},
    {3, 5},
    {2, 6},
    {1, 6},
    {0, 6},
    {-1, 6},
    {-2, 6},
    {-3, 5},
    {-4, 5},
    {-5, 4},
    {-5, 3},
    {-6, 2},
    {-6, 1},
    {-6, 0},
    {-6, -1},
    {-6, -2},
    {-5, -3},
    {-5, -4},
    {-4, -5},
    {-3, -5},
    {-2, -6},
    {-1, -6},
};
static const uint16_t circ_data_len = sizeof(circ_data) / sizeof(vec2_t);

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
    if (length % 2 == 0)
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
    point2_t const center = {track_center_new, bottom_row_idx};

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
 * @brief A callback that draws a 'light' colored pixel and stops at white.
 * @param pixels A segmented frame.
 * @param point Last point of the raycast.
 * @return 0 if cast should continue and -1 if cast should stop.
 */
static uint8_t cb_draw_light_stop_white(uint8_t (*const pixels)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH], point2_t const point)
{
    if ((*pixels)[point.y][point.x] != 255)
    {
        draw_q_pixel(point, 120);
        return 0;
    }
    return -1;
}

/**
 * @brief A callback that draws a 'light' colored pixel and does not stop at anything.
 * @param pixels A segmented frame.
 * @param point Last point of the raycast.
 * @return 0 if cast should continue and -1 if cast should stop.
 */
static uint8_t cb_draw_light_stop_no(uint8_t (*const pixels)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH], point2_t const point)
{
    draw_q_pixel(point, 120);
    return 0;
}

/**
 * @brief A callback that draws a white pixel on the frame such that it affect further computation
 * and does not stop at anything.
 * @param pixels A segmented frame.
 * @param point Last point of the raycast.
 * @return 0 if cast should continue and -1 if cast should stop.
 */
static uint8_t cb_draw_perm_stop_no(uint8_t (*const pixels)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH], point2_t const point)
{
    (*pixels)[point.y][point.x] = 255;
    return 0;
}

/**
 * @brief A callback that stops at white and does nothing else.
 * @param pixels A segmented frame.
 * @param point Last point of the raycast.
 * @return 0 if cast should continue and -1 if cast should stop.
 */
static uint8_t cb_draw_no_stop_white(uint8_t (*const pixels)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH], point2_t const point)
{
    if ((*pixels)[point.y][point.x] != 255)
    {
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
        if ((*pixels)[center.y][edge_x] == 255 || abs(center.x - edge_x) > (track_width / 2))
        {
            break;
        }
        edge_x += search_delta;
    }
    return (point2_t){edge_x, center.y};
}

/**
 * @brief Given a vector, this function determines the sweep start fraction based on the angle by
 * finding where the vector intersects with a unit circle then finding the fraction of perimeter
 * from the 0 point (up) where that intersection lies. 
 * @note It boils down to converting from a range of arctan ((0 deg) 0-Pi (180 deg) -Pi-0 (0 deg))
 * to a range of 0-1 i.e. 90 deg at 0 to wrapping around to 90 deg at 1.
 * @param vec
 * @return Sweep start fraction.
 */
static float vec_to_sweep_start(vec2_t const vec)
{
    float vec_inv_len;
    vec2_inv_length(&vec, &vec_inv_len);
    float sweep_start = (atan2f(vec.y * vec_inv_len, vec.x * vec_inv_len) / M_PI);

    if (isnan(sweep_start))
    {
        sweep_start = 0.0f;
    }

    /* Converts the 0-Pi -Pi-0 range to 0-1 as required by radial sweep. */
    if (sweep_start >= 0) /* 0 to Pi */
    {
        sweep_start = 0.25 + (sweep_start * 0.5f);
    }
    else if (sweep_start <= -0.5f) /* -Pi/2 to -Pi */
    {
        sweep_start = 0.75f + (0.25f * (1.0f - ((-sweep_start - 0.5f) * 2.0f)));
    }
    else /* 0 to -Pi/2 */
    {
        sweep_start = 0.25f * (1.0f - (-sweep_start * 2.0f));
    }

    return sweep_start;
}

/**
 * @brief Given a line (origin and direction), it finds where the line intersects with the track and
 * find the midpoint between that and the origin point.
 * @param line Line to find midpoint of. Direction must be short (ideally under 10 pixels).
 * @return Midpoint.
 */
static point2_t track_line_midpoint(uint8_t (*const pixels)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH], line2_t const line)
{
    uint16_t ray_len = raycast(pixels, (point2_t){line.orig.x + line.dir.x, line.orig.y + line.dir.y}, line.dir, &cb_draw_no_stop_white);
    if (ray_len > track_width / 2)
    {
        ray_len = track_width / 2;
    }

    vec2_t hit_vec = line.dir;
    vec2_length_change(&hit_vec, ray_len / 2);
    return (point2_t){line.orig.x + hit_vec.x, line.orig.y + hit_vec.y};
}

static void segment_track(uint8_t (*const pixels)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH], point2_t const center_black)
{
    float const sweep_start_offset = 0.1f;
    point2_t edge[2] = {track_edge(pixels, center_black, 1), track_edge(pixels, center_black, 0)};
    float edge_sweep_start[2] = {0.25f, 0.75f};
    uint8_t edge_stop[2] = {0, 0};
    uint8_t edge_diverged[2] = {0, 0};

    uint8_t const pt_num = 4;
    for (uint16_t pt_i = 0; pt_i < pt_num; pt_i++)
    {
        uint8_t sweep_status;
        /* Repeat the same for left and right. */
        for (uint8_t edge_idx = 0; edge_idx < 2; edge_idx++)
        {
            if (!edge_stop[edge_idx] && !edge_diverged[edge_idx])
            {
                point2_t const edge_last = edge[edge_idx];
                if (edge_idx == 0)
                {
                    edge[0] = radial_sweep(pixels, (vec2_t *)&circ_data, circ_data_len, edge[0], 8, 0, edge_sweep_start[0], 1.0f, &sweep_status);
                }
                else
                {
                    edge[1] = radial_sweep(pixels, (vec2_t *)&circ_data, circ_data_len, edge[1], 8, 1, edge_sweep_start[1], 1.0f, &sweep_status);
                }
                /* If something abnormal happened e.g. reached frame edge, etc... */
                if (sweep_status != 0)
                {
                    edge_stop[edge_idx] = 1;
                }
                else if (abs(edge[edge_idx].x - center_black.x) > track_width * 0.7f)
                {
                    edge_diverged[edge_idx] = 1;
                }

                /* Using vector between old and new edge point, find the ideal sweep start fraction
                for radial sweep. */
                if (edge_idx == 0)
                {
                    vec2_t const edge_sweep_start_vec = {-(edge[0].y - edge_last.y), edge[0].x - edge_last.x}; /* Normal to delta vector. */
                    edge_sweep_start[0] = vec_to_sweep_start(edge_sweep_start_vec);
                    edge_sweep_start[0] -= sweep_start_offset;
                    if (edge_sweep_start[0] < 0.0f)
                    {
                        edge_sweep_start[0] = 0.0f;
                    }
                }
                else
                {
                    vec2_t const edge_sweep_start_vec = {edge[1].y - edge_last.y, -(edge[1].x - edge_last.x)}; /* Normal to delta vector. */
                    edge_sweep_start[1] = vec_to_sweep_start(edge_sweep_start_vec);
                    edge_sweep_start[1] += sweep_start_offset;
                    if (edge_sweep_start[1] > 1.0f)
                    {
                        edge_sweep_start[1] = 1.0f;
                    }
                }
            }
        }
        if (!edge_stop[1] || !edge_stop[0])
        {
            /* The sweep start dir vector can be used directly since its length is known to be the
            circle radius which is negligible and does not need to be normalized to 1. */
            point2_t midpoint;
            if (!edge_stop[0] && edge_stop[1])
            {
                float const sweep_start = edge_sweep_start[0] + sweep_start_offset > 1.0f ? 1.0f : edge_sweep_start[0] + sweep_start_offset;
                vec2_t const sweep_start_dir = circ_data[(uint16_t)((circ_data_len - 1) * sweep_start)];
                midpoint = track_line_midpoint(pixels, (line2_t){edge[0], sweep_start_dir});
            }
            else if (edge_stop[0] && !edge_stop[1])
            {
                float const sweep_start = edge_sweep_start[1] - sweep_start_offset < 0.0f ? 0.0f : edge_sweep_start[1] - sweep_start_offset;
                vec2_t const sweep_start_dir = circ_data[(uint16_t)((circ_data_len - 1) * sweep_start)];
                midpoint = track_line_midpoint(pixels, (line2_t){edge[1], sweep_start_dir});
            }
            else
            {
                midpoint = (point2_t){(edge[0].x + edge[1].x) / 2, (edge[0].y + edge[1].y) / 2};
            }
            draw_q_square(midpoint, 4, 120);
        }
        else
        {
            break;
        }
    }
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
    segment_track(pixels, center_black);

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
