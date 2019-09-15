#include <stdio.h>
#include <stdlib.h>

#include "dynarray.h"
#include "heap.h"

struct heap {
	dynarray_t          *da;
	heap_elem_leqfn_t   *leq_fn;
	heap_elem_printfn_t *prnt_fn;
};

static int ul_leq(unsigned long e0, unsigned long e1)
{
	return (e0 <= e1) ? 1 : 0;
}

static void ul_print(unsigned long e)
{
		printf("0x%017lx ", e);
}

heap_t *heap_init(heap_elem_leqfn_t *leq_fn, heap_elem_printfn_t *prnt_fn)
{
	heap_t *h = malloc(sizeof(heap_t));
	if (!h){
		perror("malloc");
		exit(1);
	}

	if (leq_fn == NULL){
		leq_fn = ul_leq;
	}

	if (prnt_fn == NULL){
		prnt_fn = ul_print;
	}

	h->leq_fn = leq_fn;
	h->prnt_fn = prnt_fn;
	h->da = dynarray_create(sizeof(unsigned long), 128);

	return h;
}

void heap_free(heap_t *heap)
{
	void *da;

	da = dynarray_destroy(heap->da);
	free(da);
	free(heap);
}

static void siftdown(heap_t *heap, unsigned long pos, unsigned long elem)
{
	unsigned long *h = dynarray_get(heap->da, 0);
	while (pos > 0){
		unsigned long parentpos = (pos-1) >> 1;
		unsigned long parent = h[parentpos];
		if (heap->leq_fn(parent, elem)){
			break;
		}
		h[pos] = parent;
		pos = parentpos;
	}
	h[pos] = elem;
}

void heap_push(heap_t *heap, unsigned long elem)
{
	dynarray_alloc(heap->da);
	unsigned long size = dynarray_size(heap->da);
	siftdown(heap, size-1, elem);
}

void heap_print(heap_t *heap)
{
	unsigned long *h = dynarray_get(heap->da, 0);
	unsigned long size = dynarray_size(heap->da);
	unsigned long i;

	printf("--- heap at: %p\n", heap);
	for (i=0; i<size; i++){
		heap->prnt_fn(h[i]);
	}
	printf("------ end heap at %p\n", heap);
}

unsigned long heap_size(heap_t *heap)
{
	return dynarray_size(heap->da);
}

int heap_peek(heap_t *heap, unsigned long *ret)
{
	unsigned long size, *h;

	size = dynarray_size(heap->da);
	if (size == 0)
		return 0;

	h = dynarray_get(heap->da, 0);
	*ret = h[0];

	return 1;
}

/* pop an element for a specific position in the heap.
 *  The heap invariant is that all children of a node should
 *  have less priority. Thus, we need to only change the
 *  elements of the sub-tree with root position pos */
int __heap_pop(heap_t *heap, unsigned long pos, unsigned long *ret)
{
	unsigned long size = dynarray_size(heap->da);
	if (!size || pos >= size )
		return 0;
	unsigned long *h = dynarray_get(heap->da, 0);
	unsigned long last, ch_pos;

	*ret = h[pos];
	last = h[size-1];
	size--;

	for (;;){
		ch_pos = (pos<<1) + 1;
		unsigned long rch_pos = ch_pos + 1;

		if (ch_pos >= size){
			break;
		}

		if ( (rch_pos < size) && heap->leq_fn(h[rch_pos], h[ch_pos])){
			ch_pos = rch_pos;
		}

		h[pos] = h[ch_pos];
		pos = ch_pos;
	}
	/* the last element needs to be placed in the empty place,
	 * and the necessary movements performed */
	siftdown(heap, pos, last);

	dynarray_dealloc(heap->da);
	return 1;
}

int heap_pop(heap_t *heap, unsigned long *ret)
{
	return __heap_pop(heap, 0, ret);
}

/* pop a specific element */
int heap_pop_elem(heap_t *heap, unsigned long elem)
{
	dynarray_t *da = heap->da;
	unsigned long size = dynarray_size(da);
	unsigned long *array = dynarray_get(da, 0);
	unsigned long pos, e;

	for (pos = 0; pos < size; pos++) {
		if (elem == array[pos])
			return __heap_pop(heap, pos, &e);
	}
	return 0;
}

int heap_iterate(heap_t *heap, unsigned long *loc, unsigned long *val)
{
	dynarray_t *da = heap->da;
	if (*loc >= dynarray_size(da))
		return 0;

	*val = *((unsigned long *)dynarray_get(heap->da, *loc));
	*loc += 1;
	return 1;
}

#ifdef HEAP_MAIN
#define BUFLEN 1024

#include <stdio.h>

int main(int argc, char **argv)
{
	heap_t *heap;
	char *s, buf[BUFLEN];
	heap = heap_init(NULL);
	for (;;){
		s = fgets(buf, BUFLEN-1, stdin);
		if (s == NULL){
			break;
		}

		unsigned long l;
		int ret; 
		switch (*s){
			case 'I':
			l = atol(s+1);
			heap_push(heap, l);
			break;

			case 'P':
			ret = heap_pop(heap, &l);
			if (ret){
				printf("%lu\n", l);
			} else {
				printf("<empty>\n");
			}
			break;

			case 'L':
			heap_print(heap);
			break;
		}
		fflush(stdout);
	}

	heap_free(heap);
	return 0;
}
#endif
