//
// Created by vlad on 28.10.23.
//

#include <stdio.h>
#include "linked_list.h"
#include "util.h"

void lst_init(struct List *lst)
{
    lst->next = lst;
}

int
lst_empty(struct List *lst) {
    return lst->next == lst;
}

void
lst_remove(struct List *e) {
    e->prev->next = e->next;
    e->next->prev = e->prev;
}

void*
lst_pop(struct List *lst) {
    if(lst->next == lst)
        panic("lst_pop", -1);
    struct List *p = lst->next;
    lst_remove(p);
    return (void *)p;
}

void
lst_push(struct List *lst, void *p)
{
    struct List *e = (struct List *) p;
    e->next = lst->next;
    e->prev = lst;
    lst->next->prev = p;
    lst->next = e;
}

void
lst_print(struct List *lst)
{
    for (struct List *p = lst->next; p != lst; p = p->next) {
        printf(" %p", (void*)p);
    }
    printf("\n");
}

