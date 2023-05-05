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

#define HEIGHT ( 160 )
#define DEFAULT_HEIGHT ( HEIGHT + 10 )
#define RAY_LENGTH ( TCO_FRAME_WIDTH - 30 )
#define MAX_CENTERS ( 192 ) /* The maximum number of centers in the the driving algorithm */
#define WINDOW_SIZE ( 4 )

static struct tco_shmem_data_state *shmem_state;
static sem_t *shmem_sem_state;
static struct tco_shmem_data_plan *shmem_plan;
static sem_t *shmem_sem_plan;
static uint8_t shmem_state_open = 0;
static uint8_t shmem_plan_open = 0;

static uint16_t const track_width = 300; /* Pixels */
static uint16_t prev_i = 0, prev_avg = (TCO_FRAME_WIDTH / 2);
static point2_t centers[MAX_CENTERS] = {(point2_t) {(TCO_FRAME_WIDTH / 2), 180}};
static uint16_t avg_speed[WINDOW_SIZE] = {0};
static uint8_t is_finished = 0;

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
    if (shmem_map(TCO_SHMEM_NAME_STATE, TCO_SHMEM_SIZE_STATE, TCO_SHMEM_NAME_SEM_STATE, O_RDONLY, (void **)&shmem_state, &shmem_sem_state) != 0)
    {
        log_error("Failed to map state shmem into process memory");
        return EXIT_FAILURE;
    }
    if (shmem_map(TCO_SHMEM_NAME_PLAN, TCO_SHMEM_SIZE_PLAN, TCO_SHMEM_NAME_SEM_PLAN, O_RDWR, (void **)&shmem_plan, &shmem_sem_plan) != 0)
    {
        log_error("Failed to map planning shmem into process memory");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}


uint16_t add_speed_to_results(uint16_t speed) {
    for (int i = 1; i < WINDOW_SIZE; i++) {
        avg_speed[i - 1] = avg_speed[i];
    }
    avg_speed[WINDOW_SIZE - 1] = speed;
}

uint16_t find_avg_speed() {
    uint16_t res = 0;
    for (int i = 0; i < WINDOW_SIZE; i++) {
	res += avg_speed[i];
    }
    return res / WINDOW_SIZE;
}

/**
 * @brief Apply a simplified sigmoid on @param x for corners
 * @param x A float between 0 and 1
 * @return float
*/
float sigmoid_corner(float x) {
    //if (x < 0.95f) return 0.05f;
    return x;//(x - 0.3f) * 1.462f + 0.05f;
}

/**
 * @brief Apply a simplified sigmoid on @param x for straights
 * @param x A float between 0 and 1
 * @return float 
 */
float sigmoid_straight(float x) {
    if (x < 0.55f) return 0.05f;
    if (x < 0.70f) return (x - 0.15f) * 1.273f + 0.05f;
    return x;
}

/**
 * @brief Check if the car is facing a finish line. Only call when there is a possible finish line detected
 * If the top and down raycasts are of similar length and the middle raycast is twice as long, then there is a finish line.
 * @param pixels is passed as a ptr
 * @return int > 0 if there is a finish
 */
int finish_condition(uint8_t (*pixels)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH],
    point2_t center,
    vec2_t dir,
    uint16_t parallel_dir_length,
    uint16_t left_diagonal,
    uint16_t right_diagonal,
    uint16_t left_horizontal,
    uint16_t right_horizontal) {

    uint16_t perp_length_r = raycast(pixels, center, (vec2_t) {-dir.y, dir.x}, &cb_draw_light_stop_white);
    uint16_t perp_length_l = raycast(pixels, center, (vec2_t) {dir.y, -dir.x}, &cb_draw_light_stop_white);

    static const uint8_t max_ray_length = 120;
    static const uint8_t max_length_hor = 80;
    static const float min_rc = 4.0f;
    if (fabs(-dir.y / (float) dir.x) < min_rc || ((perp_length_r + perp_length_l) < 20 )) return 0;
    //printf("%f, %d %d %d\n", -dir.y / (float) dir.x,left_diagonal, right_diagonal, parallel_dir_length);
    return (((left_diagonal + right_diagonal) > max_ray_length) && 
            ((left_horizontal + right_horizontal) < max_length_hor) &&
            (parallel_dir_length > max_ray_length)) ||
            (((left_diagonal + right_diagonal) < 90) && ((left_diagonal < 40) || (right_diagonal < 40)) && (parallel_dir_length > max_ray_length));
}

/**
 * @brief Calculate the best position to be in according to the current *segmented* frame
 * @param pixels is passed as a ptr
 * @param target_pos is the desired position (-1 left edge, 1 right edge, 0 center) of current frame
 * @param target_speed is the speed to go at (m/s). NOTE This unit can easily be changed
 * @return void. values are passed through @p target_pos and @p target_speed pointers. 
 */
void calculate_next_position(uint8_t (*pixels)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH], float *target_pos, float *target_speed) {
    *target_pos = 0.0f; 
    *target_speed = 0.0f;
    uint16_t speed, height = HEIGHT, i = 0, sum_x = 0, sum_y = 0, avg_x = prev_avg;
    uint32_t sum_of_rays = 0;
    static const uint16_t step = 8;
    static const uint16_t intersection_avg = 450;
    static const uint16_t intersection_sum = 14000;
    static const float height_limit = 1.5f;
    point2_t base;

    point2_t center_track = base = track_center(pixels, height, prev_avg);
    uint16_t straight = speed = raycast(pixels, (point2_t) {center_track.x, DEFAULT_HEIGHT}, (vec2_t) {0,-1}, &cb_draw_light_stop_white); 
    uint16_t left_diagonal = raycast(pixels, (point2_t) {prev_avg, DEFAULT_HEIGHT}, (vec2_t) {-1,-1}, &cb_draw_light_stop_white);
    uint16_t right_diagonal = raycast(pixels, (point2_t) {prev_avg, DEFAULT_HEIGHT}, (vec2_t) {1,-1}, &cb_draw_light_stop_white);
    uint16_t left_horizontal =  raycast(pixels, (point2_t) {prev_avg, DEFAULT_HEIGHT}, (vec2_t) {-1,0}, &cb_draw_light_stop_white);  
    uint16_t right_horizontal = raycast(pixels, (point2_t) {prev_avg, DEFAULT_HEIGHT}, (vec2_t) {1,0}, &cb_draw_light_stop_white);
    add_speed_to_results(speed);

    for (height = HEIGHT, i = 0; (height > step) && (straight > (height_limit * step)) && i < MAX_CENTERS; height -= step, i++) {
        straight = i > 0 ? raycast(pixels, centers[i - 1], (vec2_t) {0,-1}, &cb_draw_no_stop_white) : straight;

//        if (straight <= (height_limit * step)) {
//            if (left_diagonal > (height_limit * right_diagonal)) {
//                centers[i - 1].x -= (TCO_FRAME_WIDTH / 4);
//           } else if (right_diagonal > (height_limit * left_diagonal)) {
//                centers[i - 1].x += (TCO_FRAME_WIDTH / 4);
//            }
//            straight = i > 0 ? raycast(pixels, centers[i - 1], (vec2_t) {0,-1}, &cb_draw_no_stop_white) : straight;
//        }

        center_track = centers[i] = i != 0 ? track_center(pixels, height, centers[i - 1].x) : track_center(pixels, height, centers[0].x);

        uint16_t ray_right = raycast(pixels, center_track, (vec2_t) {1, 0}, &cb_draw_no_stop_white);
        uint16_t ray_left = raycast(pixels, center_track, (vec2_t) {-1, 0}, &cb_draw_no_stop_white);

        draw_q_square(center_track, 4, 128);

        sum_x += center_track.x;
        sum_y += center_track.y;
        sum_of_rays += ray_left + ray_right;

    }    

    prev_i = (abs(i - prev_i) < 4) ? i : prev_i;
    uint16_t avg_rays =  i > 0 ? sum_of_rays / i : sum_of_rays;

    vec2_t parallel_extremes = { center_track.x - base.x, center_track.y - base.y };
    // raycast(pixels, base, parallel_extremes, &cb_draw_light_stop_white);
    // printf("%d %d\n", avg_rays, sum_of_rays);
    center_track.x = avg_x = i > 0 ? sum_x / i : track_center(pixels, height, centers[0].x).x;
    center_track.y = DEFAULT_HEIGHT;

    /* Take the gradient of all the centers (parallel_dir.y / parallel_dir.x) and the two extremes (parallel_extremes.y / parallel_extremes.x) */
    /* Speed is determined by distance to edge of track */
    vec2_t parallel_dir = { center_track.x - base.x, (i > 0 ? sum_y / i : HEIGHT - 10) - base.y };

    uint16_t parallel_dir_length = raycast(pixels, base, parallel_dir, &cb_draw_no_stop_white);

    if (i > 5 && finish_condition(pixels, center_track, parallel_dir, parallel_dir_length, left_diagonal, right_diagonal, left_horizontal, right_horizontal)) is_finished = 1;
    if (is_finished) draw_q_number(1, (point2_t) {10,40}, 4);
    else draw_q_number(0, (point2_t) {10,40}, 4);

    //if ((fabs(parallel_dir.y / (double) parallel_dir.x) < 5.0f) ||
    //    (fabs(parallel_extremes.y / (double) parallel_extremes.x) < 5.0f)) {
    //       *target_speed = sigmoid_corner(find_avg_speed() / ((float) DEFAULT_HEIGHT));
    //    }
    //else {
    //    *target_speed = sigmoid_corner(find_avg_speed() / ((float) DEFAULT_HEIGHT));
    //}
    draw_q_number((int) ((*target_speed * 100.0f)), (point2_t) {10, 10}, 4);
    
    float average_steering = (prev_avg + avg_x) / 2.0f;
    // TODO: good check
    prev_avg = avg_x;//(abs(avg_x - prev_avg) < (TCO_FRAME_WIDTH / 4)) ? avg_x : prev_avg;

    draw_q_square(center_track, 12, 128);
    
    *target_pos = (average_steering - (TCO_FRAME_WIDTH / 2.0f)) / (TCO_FRAME_WIDTH / 2.0f);
    *target_pos = *target_pos * 4.0f;
    *target_speed = sigmoid_corner(find_avg_speed() / ((float) DEFAULT_HEIGHT));
}


int plnr_step(uint8_t (*const pixels)[TCO_FRAME_HEIGHT][TCO_FRAME_WIDTH])
{
    /* Calculate the next coordinate */
    float target_pos = 0, target_speed = 0;
    calculate_next_position(pixels, &target_pos, &target_speed);

    if (sem_wait(shmem_sem_plan) == -1)
    {
        log_error("sem_wait: %s", strerror(errno));
        return EXIT_FAILURE;
    }
    /* START: Critical section */
    shmem_plan_open = 1;
    shmem_plan->target_pos = target_pos;
    shmem_plan->target_speed = target_speed;
    shmem_plan->lap_of_honor = is_finished;
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
    if (shmem_state_open)
    {
        if (sem_post(shmem_sem_state) == -1)
        {
            log_error("sem_post: %s", strerror(errno));
            return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}
