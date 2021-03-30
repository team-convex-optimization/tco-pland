#include <string.h>

#include "tco_libd.h"

#include "sort.h"

uint8_t comp_u16(void *const a, void *const b)
{
    int32_t const dif = *((uint16_t *)a) - *((uint16_t *)b);
    uint8_t ret = 0;
    if (dif == 0)
    {
        ret += COMP_EQUAL;
    }
    if (dif > 0)
    {
        ret += COMP_GREATER;
    }
    return ret;
}

int insertion_sort_integer(uint8_t *const data, uint16_t const el_count, uint8_t const el_size, uint8_t (*comparator)(void *const, void *const))
{
    for (int i = 1; i < el_count; i++)
    {
        for (int j = i; j > 0 && (comparator(&data[j * el_size], &data[(j - 1) * el_size]) ^ COMP_GREATER) == 0; j--)
        {
            /* Swapping [j] and [j-1] */
            uint8_t el_to_swap[el_size];
            memcpy(&el_to_swap, &data[j * el_size], el_size);
            memcpy(&data[j * el_size], &data[(j - 1) * el_size], el_size);
            memcpy(&data[(j - 1) * el_size], &el_to_swap, el_size);
        }
    }

    return 0;
}
