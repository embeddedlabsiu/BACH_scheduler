#ifndef __C_QSORT_H__
#define __C_QSORT_H__

#include <stdlib.h>
#include "../aff-executor.h"
#include "../list.h"


enum sort_type {ASC, DESC};
extern int (*qsrt_get_val)(list_t *l);

list_t * qsrt_partition(list_t *list, enum sort_type qtype);
void qsrt_quicksort(list_t *list, enum sort_type qtype);


#endif


