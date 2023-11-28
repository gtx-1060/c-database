//
// Created by vlad on 28.10.23.
//

#ifndef LAB1_LINKED_LIST_H
#define LAB1_LINKED_LIST_H

typedef struct List {
    struct List* next;
    struct List* prev;
} List;

void lst_init(struct List *lst);
int lst_empty(struct List *lst);
void lst_remove(struct List *e);
void* lst_pop(struct List *lst);
void lst_push(struct List *lst, void *p);
void lst_print(struct List *lst);

#endif //LAB1_LINKED_LIST_H
