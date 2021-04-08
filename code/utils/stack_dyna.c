#include <stdlib.h>

#include <string.h>

#include "stack_dyna.h"

int stack_dyna_push(stack_dyna_t *const stack, void const *const data)
{
    if (stack->data == NULL)
    {
        stack->data = malloc(4096); /* Get a page. */
        stack->data_len = 4096;
        stack->top = 0;
    }
    else if (stack->top * stack->el_size >= stack->data_len)
    {
        stack->data = realloc(stack->data, stack->data_len * stack->el_size + 4096);
    }
    memcpy(stack->data + (stack->top++) * stack->el_size, data, stack->el_size);
    return 0;
}

int stack_dyna_pop(stack_dyna_t *const stack)
{
    if (stack->top == 0)
    {
        return -1;
    }
    stack->top--;
    return 0;
}

int stack_dyna_top(stack_dyna_t *const stack, void *const top)
{
    if (stack->top == 0)
    {
        return -1;
    }
    memcpy(top, stack->data + (stack->top - 1) * stack->el_size, stack->el_size);
    return 0;
}
