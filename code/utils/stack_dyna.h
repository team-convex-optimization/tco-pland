#ifndef _STACK_DYNA_H_
#define _STACK_DYNA_H_

#include <stdint.h>

/**
 * @brief Dynamically sized stack.
 * @note @p data must be allocated with the system allocator (malloc, calloc, or realloc).
 */
typedef struct stack_dyna
{
    void *data;
    uint16_t data_len;
    uint16_t top;    /* Points to the location where the next element will be pushed. When at 0, stack is empty. */
    uint8_t el_size; /* Mandatory field. */
} stack_dyna_t;

/**
 * @brief Push an element onto the stack.
 * @note If the stack structure contains a null pointer to data, then a memory region will be
 * allocated on the heap and the pointer stored in the structure.
 * @param stack
 * @param data Must contain at least as many bytes as the element size of the stack.
 * @return 0 on success and -1 on failure.
 */
int stack_dyna_push(stack_dyna_t *const stack, void const *const data);

/**
 * @brief Pop an element off the stack. The popped element will not be returned. To get the popped
 * element, read the top value before popping.
 * @param stack
 * @return 0 on success and -1 on failure.
 */
int stack_dyna_pop(stack_dyna_t *const stack);

/**
 * @brief Get the top element on the stack.
 * @param stack
 * @param top Where the top element will be written.
 * @return 0 on success and -1 on failure.
 */
int stack_dyna_top(stack_dyna_t *const stack, void *const top);

#endif /* _STACK_DYNA_H_ */