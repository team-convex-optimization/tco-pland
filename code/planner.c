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

static uint8_t const track_center_count = 4;
static uint16_t track_centers_data[track_center_count] = {0};
static buf_circ_t track_centers = {track_centers_data, track_center_count, track_center_count - 1, sizeof(uint16_t)};

static uint8_t const track_distances_count = 6;
static uint16_t track_distances_data[track_distances_count] = {0};
static buf_circ_t track_distances = {track_distances_data, track_distances_count, track_distances_count - 1, sizeof(uint16_t)};

static uint16_t ray_length_last = 0;
static point2_t ray_hit = {0, 0};

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
        ray_length_last += 1;
        ray_hit.x = point.x;
        ray_hit.y = point.y;
        return 0;
    }
    return -1;
}

/**
 * @brief Start a raycast from a @p start position in the direction of @p dir .
 * @param pixels Frame where the raycast will be shot. It needs to be a segmented frame.
 * @param start Where the raycast will begin.
 * @param dir In what direction the ray will be cast.
 * @return Length of the raycast.
 */
static uint16_t raycast(uint8_t (*const pixels)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH], point2_t const start, vec2_t const dir)
{
    /* How much to stretch the direction vector so it touches the frame border. */
    float const edge_stretch_x = dir.x < 0 ? start.x / fabs((float)dir.x) : (TCO_FRAME_WIDTH - 1 - start.x) / fabs((float)dir.x);
    float const edge_stretch_y = dir.y < 0 ? start.y / fabs((float)dir.y) : (TCO_FRAME_HEIGHT - 1 - start.y) / fabs((float)dir.y);
    float const edge_stretch = edge_stretch_y < edge_stretch_x ? edge_stretch_y : edge_stretch_x;

    /* A direction vector which when added to start goes to the border of the frame while keeping
    angle. */
    vec2_t const dir_stretched = {dir.x * edge_stretch, dir.y * edge_stretch};
    point2_t const end = {start.x + dir_stretched.x, start.y + dir_stretched.y};

    bresenham(pixels, &raycast_callback, (point2_t){start.x, start.y}, end);
    draw_q_square(ray_hit, 10, 120);

    uint16_t ray_length_tmp = ray_length_last;
    ray_length_last = 0;

    return ray_length_tmp;
}

/**
 * @brief Perform a fast but rough radial sweep contour trace. It will trace at most @p
 * contour_length pixels and will travel in @p cw_or_ccw (clockwise or counter-clockwise) direction.
 * @param pixels The frame.
 * @param start Where the the tracing should start from.
 * @param contour_length Max number of traced pixels.
 * @param cw_or_ccw Begin tracing clockwise or counter-clockwise.
 * @return Last point traced.
 */
static point2_t radial_sweep(uint8_t (*const pixels)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH], point2_t const start, uint16_t const contour_length, uint8_t const cw_or_ccw)
{
    static uint16_t const trace_margin = 10;
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
    uint16_t const circ_size = sizeof(circ_data) / sizeof(vec2_t);
    uint16_t const quarter_size = circ_size / 4;
    uint16_t const circ_idx_90deg = quarter_size + quarter_size + quarter_size;
    uint16_t const circ_idx_270deg = quarter_size;

    buf_circ_t circ_buf = {(void *)circ_data, circ_size, circ_size - 1, sizeof(vec2_t)};
    uint16_t circ_idx = 0;
    point2_t trace_last = start;
    if (cw_or_ccw)
    {
        circ_idx = circ_idx_90deg;
    }
    else
    {
        circ_idx = circ_idx_270deg;
    }
    for (uint16_t contour_length_now = 0; contour_length_now < contour_length; contour_length_now++)
    {
        for (uint16_t swept_pts = 0; swept_pts < circ_size - 1; swept_pts++)
        {
            vec2_t const circ_vec = *((vec2_t *)buf_circ_get(&circ_buf, circ_idx));
            point2_t const trace_target = {trace_last.x + circ_vec.x, trace_last.y + circ_vec.y};

            /* Outside bounds */
            if (trace_target.y >= TCO_FRAME_HEIGHT - trace_margin ||
                trace_last.x >= TCO_FRAME_WIDTH - trace_margin ||
                trace_last.x <= trace_margin ||
                trace_target.y <= trace_margin)
            {
                return trace_last;
            }
            draw_q_pixel(trace_target, 120);

            /* End current sweep when a white point is found and move onto the next one. */
            if ((*pixels)[trace_target.y][trace_target.x] == 255)
            {
                trace_last = trace_target;
                /* Sweep from a normal to the current point. */
                if (cw_or_ccw)
                {
                    circ_idx = circ_idx + circ_idx_90deg;
                }
                else
                {
                    circ_idx = circ_idx + circ_idx_270deg;
                }
                break;
            }

            /* Go to next circle point. */
            if (cw_or_ccw)
            {
                circ_idx += 1;
            }
            else
            {
                circ_idx += circ_size - 1; /* "buf_circ_get" will wrap-around automatically. */
            }

            /* If swept through whole circle (excluding starting pos) without a trace, just end
            prematurely.  */
            if (swept_pts >= circ_size - 1)
            {
                return trace_last;
            }
        }
    }
    return trace_last;
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
    while (edge_left.x > 0)
    {
        if ((*pixels)[edge_left.y][edge_left.x] == 255)
        {
            break;
        }
        edge_left.x -= 1;
    }
    while (edge_right.x < TCO_FRAME_WIDTH)
    {
        if ((*pixels)[edge_right.y][edge_right.x] == 255)
        {
            break;
        }
        edge_right.x += 1;
    }

    draw_q_square(edge_left, 4, 120);
    draw_q_square(edge_right, 4, 120);

    point2_t edge_left_trace = edge_left;
    point2_t edge_right_trace = edge_right;

    uint8_t edge_left_missing = 1;
    uint8_t edge_right_missing = 1;

    if ((*pixels)[edge_left.y][edge_left.x] == 255)
    {
        edge_left_trace = radial_sweep(pixels, edge_left, 10, 0);
        edge_left_missing = 0;
    }
    if ((*pixels)[edge_right.y][edge_right.x] == 255)
    {
        edge_right_trace = radial_sweep(pixels, edge_right, 10, 1);
        edge_right_missing = 0;
    }

    /* If an edge is missing, the corresponding vector will be {0,0}. */
    vec2_t edge_left_dir = {edge_left_trace.x - edge_left.x, edge_left_trace.y - edge_left.y};
    vec2_t edge_right_dir = {edge_right_trace.x - edge_right.x, edge_right_trace.y - edge_right.y};

    vec2_t track_dir_estimated;
    if (!edge_left_missing && !edge_right_missing)
    {
        /* No edges missing so average of the 2 will be a good estimation of track direction. */
        track_dir_estimated.x = (edge_left_dir.x + edge_right_dir.x) / 2;
        track_dir_estimated.y = (edge_left_dir.y + edge_right_dir.y) / 2;
    }
    else if (edge_left_missing && edge_right_missing)
    {
        /* No edges detected. */
        track_dir_estimated.x = 0;
        track_dir_estimated.y = -1; /* Defaulting to straight ahead. */
    }
    else if (edge_right_missing)
    {
        /* Right edge missing so left is used directly. */
        track_dir_estimated = edge_left_dir;
    }
    else
    {
        /* Left edge missing so right is used directly. */
        track_dir_estimated = edge_right_dir;
    }

    /* TODO: Apply direction heuristics. */
    return track_dir_estimated;
}

/**
 * @brief Track distances from the car to the furthest point ahead on the track if measured parallel
 * to the track.
 * @param pixels A segmented frame.
 * @param center Starting point of the raycast.
 * @return Distance ahead of the car to the edge of the track (mesures paralell to the track along
 * the centerline).
 */
static uint16_t track_distance(uint8_t (*const pixels)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH], point2_t const center)
{
    point2_t const center_black = {center.x, center.y - 10};
    vec2_t const dir_track = track_orientation(pixels, center_black);

    /* Normalize the track direction such that it is approximately the length of the other direction vectors. */
    float const dir_track_inv_dir_length = sqrt(2 * 2 + -3 * -3) / sqrt(dir_track.x * dir_track.x + dir_track.y * dir_track.y);
    vec2_t const dir_track_short = {dir_track.x * (dir_track_inv_dir_length), dir_track.y * dir_track_inv_dir_length};

    raycast(pixels, center_black, dir_track);
    vec2_t const dirs[] = {
        {-2, -3},
        {-1, -3},
        {0, -3},
        {1, -3},
        {2, -3},
    };
    uint16_t dirs_num = sizeof(dirs) / sizeof(vec2_t);

    uint16_t distance_total = 0;
    for (uint16_t dir_idx = 0; dir_idx < dirs_num; dir_idx++)
    {
        distance_total += raycast(pixels, center_black, (vec2_t){dirs[dir_idx].x + dir_track_short.x, dirs[dir_idx].y});
    }
    distance_total /= dirs_num;
    buf_circ_add(&track_distances, (uint8_t *)&distance_total);
    return listu16_median(track_distances_data, track_distances_count);
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
    uint16_t distance_ahead = track_distance(pixels, center);
    draw_q_number(distance_ahead, (point2_t){10, 100}, 4);

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
