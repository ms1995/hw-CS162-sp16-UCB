/*
 * mm_alloc.c
 *
 * Stub implementations of the mm_* routines.
 */

#include "mm_alloc.h"
#include <stdlib.h>
#include <unistd.h>
#include <memory.h>

size_t heap_size = 0;
void *heap_start = NULL;

struct data_block
{
	size_t size;
	char is_free;
	struct data_block *prev, *next;
	char data[0];
};

void *mm_malloc(size_t size) {
	if (size == 0)
		return NULL;
    if (heap_start == NULL)
    {
    	heap_start = sbrk(sizeof(struct data_block));
    	memset(heap_start, 0, sizeof(struct data_block));
    }
    size_t total_size = sizeof(struct data_block) + size;
    struct data_block *curr_block = (struct data_block *) heap_start;
    while (curr_block->size < size || !curr_block->is_free)
    {
    	if (curr_block->next == NULL)
    	{
    		struct data_block *new_block = sbrk(total_size);
    		if (new_block < 0)
    			return NULL;
    		memset(new_block, 0, total_size);
    		new_block->prev = curr_block;
    		new_block->size = size;
    		new_block->is_free = 1;
    		curr_block->next = new_block;
    	}
    	curr_block = curr_block->next;
    }
    if (curr_block->size > total_size + sizeof(struct data_block))
    {
    	struct data_block *new_block = (struct data_block *) (curr_block + total_size);
    	size_t new_size = curr_block->size - size;
    	memset(new_block, 0, new_size);
    	new_block->size = new_size;
    	new_block->is_free = 1;
    	new_block->prev = curr_block;
    	new_block->next = curr_block->next;
    	curr_block->next->prev = new_block;
    	curr_block->next = new_block;
    }
    return curr_block->data;
}

void *mm_realloc(void *ptr, size_t size) {
    /* YOUR CODE HERE */
    return NULL;
}

void mm_free(void *ptr) {
    /* YOUR CODE HERE */
}
