#ifndef _CAMERA_H_
#define _CAMERA_H_

#include <stdint.h>

typedef struct frame_processor_t
{
    void (*func)(uint8_t *, int, void *); /* Pixels | Length | Args */
    void *args;
} frame_processor_t;

typedef struct frame_injector_t
{
    void (*func)(uint8_t *, int, void *); /* Pixel destination | Length | Args */
    void *args;
} frame_injector_t;

typedef struct camera_user_data_t
{
    frame_processor_t frame_processor_data;
    frame_injector_t frame_injector_data;
} camera_user_data_t;

int camera_pipeline_run(camera_user_data_t *const user_data);

int camera_sim_pipeline_run(camera_user_data_t *const user_data);

int display_pipeline_run(camera_user_data_t *const user_data);

#endif /* _CAMERA_H_ */