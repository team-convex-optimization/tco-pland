#include <string.h>

#include "tco_libd.h"

#include "buf_circ.h"

void buf_circ_add(buf_circ_t *const buf, void *const el)
{
    buf->el_idx_last += 1;
    buf->el_idx_last %= buf->el_num;
    memcpy(&buf->data[buf->el_idx_last * buf->el_size], el, buf->el_size);
}

void *buf_circ_get(buf_circ_t *const buf, uint16_t const idx)
{
    return &buf->data[(idx % buf->el_num) * buf->el_size];
}
