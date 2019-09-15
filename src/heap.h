#ifndef HEAP_H__
#define HEAP_H__

struct heap;
typedef struct heap heap_t;

typedef int heap_elem_leqfn_t(unsigned long e0, unsigned long e1);
typedef void heap_elem_printfn_t(unsigned long e);

heap_t *heap_init(heap_elem_leqfn_t *cmpfn, heap_elem_printfn_t *prnfn);
int heap_peek(heap_t *heap, unsigned long *ret);
void heap_push(heap_t *heap, unsigned long elem);
int heap_pop(heap_t *heap, unsigned long *elem);
int __heap_pop(heap_t *heap, unsigned long pos, unsigned long *elem);
int heap_pop_elem(heap_t *heap, unsigned long elem);
unsigned long heap_size(heap_t *heap);
void heap_free(heap_t *heap);
int heap_iterate(heap_t *heap, unsigned long *loc, unsigned long *val);
void heap_print(heap_t *heap);


#endif /* HEAP_H__ */
