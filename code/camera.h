#ifndef _CAMERA_H_
#define _CAMERA_H_

#include <stdint.h>

typedef struct camera_user_data_t
{
    void (*f)(uint8_t *, int, void *); /* Pixels | Length | Args */
    void *args;
} camera_user_data_t;

int camera_pipeline_run(int argc, char *argv[], camera_user_data_t *user_data);

#endif /* _CAMERA_H_ */