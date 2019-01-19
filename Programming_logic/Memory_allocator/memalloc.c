/*
 * Author: coppermonkey
 *
 * Custom memory allocators
 * use: export LD_PRELOAD=`pwd`/memalloc.so
 * to use in current shell.
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

struct header_t
{
	size_t size;
	unsigned is_free;
	struct header_t *next;
};

pthread_mutex_t global_malloc_lock;
struct header_t *head, *tail;

struct header_t *get_free_block(size_t size)
{
	struct header_t *curr = head;
	while(curr) {
		if (curr->is_free && curr->size >= size)
			return curr;
		curr = curr->next;
	}
	return NULL;
}

void *malloc(size_t size)
{
	size_t total_size;
	void *block;
	struct header_t *header;

	if (!size)
		return NULL;

	pthread_mutex_lock(&global_malloc_lock);
	header = get_free_block(size);

	if (header) {
		header->is_free = 0;
		pthread_mutex_unlock(&global_malloc_lock);
		return (void*)(header + 1);
	}
	total_size = sizeof(struct header_t) + size;
	block = sbrk(total_size);
	if (block == (void*) -1) {
		pthread_mutex_unlock(&global_malloc_lock);
		return NULL;
	}
	header = block;
	header->size = size;
	header->is_free = 0;
	header->next = NULL;
	if (!head)
		head = header;
	if (tail)
		tail->next = header;
	tail = header;
	pthread_mutex_unlock(&global_malloc_lock);
	return (void*)(header + 1);
}

void free(void *block)
{
	struct header_t *header, *tmp;
	void *programbreak;

	if (!block)
		return;
	pthread_mutex_lock(&global_malloc_lock);
	header = (struct header_t*)block - 1;

	programbreak = sbrk(0);
	if ((char*)block + header->size == programbreak) {
		if (head == tail) {
			head = tail = NULL;
		} else {
			tmp = head;
			while (tmp) {
				if(tmp->next == tail) {
					tmp->next = NULL;
					tail = tmp;
				}
				tmp = tmp->next;
			}
		}
		sbrk(0 - sizeof(struct header_t) - header->size);
		pthread_mutex_unlock(&global_malloc_lock);
		return;
	}
	header->is_free = 1;
	pthread_mutex_unlock(&global_malloc_lock);
}

void *calloc(size_t num, size_t nsize)
{
	size_t size;
	void *block;
	if (!num || !nsize)
		return NULL;
	size = num * nsize;
	/* check mul overflow */
	if (nsize != size / num)
		return NULL;
	block = malloc(size);
	if (!block)
		return NULL;
	memset(block, 0, size);
	return block;
}

void *realloc(void *block, size_t size)
{
	struct header_t *header;
	void *ret;
	if (!block || !size)
		return malloc(size);
	header = (struct header_t*)block - 1;
	if (header->size >= size)
		return block;
	ret = malloc(size);
	if (ret) {

		memcpy(ret, block, header->size);
		free(block);
	}
	return ret;
}
