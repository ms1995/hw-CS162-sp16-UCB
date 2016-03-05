/*
 * mm_alloc.c
 *
 * Stub implementations of the mm_* routines.
 */

#include "mm_alloc.h"
#include <stdlib.h>
#include <unistd.h>
#include <memory.h>

void *heap_start = NULL;

struct data_block
{
	size_t size;
	char is_free;
	struct data_block *prev, *next;
	char data[0];
};

void *mm_malloc(size_t size) {
    size_t total_size = sizeof(struct data_block) + size;
    if (total_size <= sizeof(struct data_block)) // overflow, or size=0
    	return NULL;
    if (heap_start == NULL)
    {
    	heap_start = sbrk(sizeof(struct data_block));
    	memset(heap_start, 0, sizeof(struct data_block));
    }
    struct data_block *curr_block = (struct data_block *) heap_start;
    while (curr_block->size < size || !curr_block->is_free)
    {
    	if (curr_block->next == NULL)
    	{
    		struct data_block *new_block = sbrk(total_size);
    		if (new_block == -1)
    			return NULL;
    		new_block->prev = curr_block;
    		new_block->next = NULL;
    		new_block->size = size;
    		new_block->is_free = 1;
    		curr_block->next = new_block;
    	}
    	curr_block = curr_block->next;
    }
    if (curr_block->size > total_size)
    {
    	struct data_block *new_block = (struct data_block *) ((void*)curr_block + total_size);
    	size_t new_size = curr_block->size - total_size;
    	new_block->size = new_size;
    	new_block->is_free = 1;
    	new_block->prev = curr_block;
    	new_block->next = curr_block->next;
    	if (curr_block->next != NULL)
    		curr_block->next->prev = new_block;
    	curr_block->next = new_block;
    	curr_block->size = size;
    }
    curr_block->is_free = 0;
    memset(curr_block->data, 0, size);
    return curr_block->data;
}

void *mm_realloc(void *ptr, size_t size) {
    /* YOUR CODE HERE */
    return NULL;
}

void mm_free(void *ptr) {
    if (ptr == NULL || heap_start == NULL || ptr < heap_start)
    	return;
    struct data_block *curr_block = (struct data_block *) heap_start;
    do
    {
    	curr_block = curr_block->next; // skip the first virtual block
    	if (curr_block == NULL)
    		return;
    }
    while (curr_block->data + curr_block->size <= ptr);
    curr_block->is_free = 1;
    if (curr_block->prev->is_free)
    {
    	// merge left
    	curr_block->prev->next = curr_block->next;
    	if (curr_block->next != NULL)
    		curr_block->next->prev = curr_block->prev;
    	curr_block->prev->size += curr_block->size + sizeof(struct data_block);
    	curr_block = curr_block->prev;
    }
    if (curr_block->next != NULL && curr_block->next->is_free)
    {
    	// merge right
    	curr_block->size += curr_block->next->size + sizeof(struct data_block);
    	if (curr_block->next->next != NULL)
    		curr_block->next->next->prev = curr_block;
    	curr_block->next = curr_block->next->next;
    }
}
