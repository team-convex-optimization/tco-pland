#ifndef _SORT_H_
#define _SORT_H_

#include <stdint.h>

/* Flags that must be present in the output of comparator functions. */
enum comp_flag
{
    COMP_EQUAL = 0b1,    /* 1=equal and 0=unequal */
    COMP_GREATER = 0b01, /* 1=greater and 0=smaller */
};

/**
 * @brief Performs insertion sort and allows the sorted array to be of any integer type. The
 * operation is done in-place so output is stored in the input array.
 * @param data The array to be sorted.
 * @param data_size Number of elements in @p data .
 * @param el_size Size of every element in @p data in bytes.
 * @param comparator The function which will compare elements in @p data .
 * @return 0 on success and -1 on failure.
 */
int insertion_sort_integer(uint8_t *const data, uint16_t const el_count, uint8_t const el_size, uint8_t (*comparator)(void *const, void *const));

/**
 * @brief Compares two 16bit numbers.
 * @param a First number.
 * @param b Second number.
 * @return COMP_EQ iff a==b, COMP_SMALLER if a<b, COMP_GREATER if a>b, else COMP_UNDEF.
 */
uint8_t comp_u16(void *const a, void *const b);

#endif /* _SORT_H_ */