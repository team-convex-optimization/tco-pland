#ifndef _LINALG_H_
#define _LINALG_H_

#include <stdint.h>

typedef struct point2_t
{
    uint16_t x;
    uint16_t y;
} point2_t;

typedef struct vec2_t
{
    int16_t x;
    int16_t y;
} vec2_t;

typedef struct line
{
    uint8_t valid;
    point2_t bot;
    point2_t top;
} line_t;

#endif /* _LINALG_H_ */
