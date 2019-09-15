#include "c_qsort.h"

int (*qsrt_get_val)(list_t *l);

list_t * qsrt_partition(list_t *list, enum sort_type qtype){
        int pivot, flag;
        list_t *prev, *tmp, *i, *j;

        pivot = qsrt_get_val(list->next);
        i = j = list;

	flag = 0;
        while (1) {
                do {
                        i = i->next;
                        if ((i->prev == j || i == j) && j != list) flag = 1;
                } while (((qtype == DESC) && (qsrt_get_val(i) > pivot)) || ((qtype == ASC) && (qsrt_get_val(i) < pivot)));

                do {
                        j = j->prev;
                        if ((j->next == i || i == j) && i != list) flag = 1;
                } while (((qtype == DESC) && (qsrt_get_val(j) < pivot)) || ((qtype == ASC) && (qsrt_get_val(j) > pivot)));

                if (!flag) {
                        prev = i->prev;
                        cds_list_del(i);
                        cds_list_add(i, j);
                        cds_list_del(j);
                        cds_list_add(j, prev);
                        tmp = i;
                        i = j;
                        j = tmp;
                } else {
                        return j;
                }
        }
}

#define singleton(_list) ((_list)->next->next == (_list))

void qsrt_quicksort(list_t *list, enum sort_type qtype){
        list_t *q, *head, *tail;

        if (singleton(list)) return;

        head = malloc(sizeof(*head));
        if (head == NULL) {
                //perrdie("malloc", "");
                exit(1);
        }

        tail = malloc(sizeof(*tail));
        if (tail == NULL) {
                //perrdie("malloc", "");
                exit(1);
        }

        q = qsrt_partition(list, qtype);

        /* head is the first list */
        head->next = list->next;
        head->prev = q;
        /* tail is the second one */
        tail->next = q->next;
        tail->prev = list->prev;

        /* temporary change list heads for  our lists elements */
        head->next->prev = head;
        head->prev->next = head;
        tail->next->prev = tail;
        tail->prev->next = tail;

        /* quicksort two lists */
        qsrt_quicksort(head, qtype);
        qsrt_quicksort(tail, qtype);

        /* make list consistent again */
        list->next = head->next;
        head->next->prev = list;
        tail->next->prev = head->prev;
        head->prev->next = tail->next;
        list->prev = tail->prev;
        tail->prev->next = list;
}

                                             

