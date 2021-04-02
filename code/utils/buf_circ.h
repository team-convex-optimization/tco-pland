#ifndef _BUF_CIRC_H_
#define _BUF_CIRC_H_

/* Circular Buffer */

#include <stdint.h>

typedef struct buf_circ
{
    void *data;
    uint16_t el_num;
    uint16_t el_idx_last;
    uint8_t el_size;
} buf_circ_t;

/**
 * @brief Write an element after the last written element and wraparound to the beginning if needed.
 * @param buf Circular buffer where the element will be written.
 * @param el Element to write to buffer.
 */
void buf_circ_add(buf_circ_t *const buf, void *const el);

/**
 * @brief Get a pointer to an element at a given index.
 * @param buf Circular buffer to return a pointer to.
 * @param idx Index of the element to get. If index is too large, it gets wrapped around.
 * @return Pointer to an element at index @p idx .
 */
void *buf_circ_get(buf_circ_t *const buf, uint16_t const idx);

#endif /* _BUF_CIRC_H_ */