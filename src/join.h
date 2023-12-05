//
// Created by vlad on 04.12.23.
//

#ifndef LAB1_JOIN_H
#define LAB1_JOIN_H

#include "request_iterator.h"

typedef struct Join {
    RequestIterator* iter1;
    RequestIterator* iter2;
    uint16_t row1_len;
    uint16_t row2_len;
    char* fname;
    uint16_t field_ind_t1;
    uint16_t field_ind_t2;
    void** row;
} Join;

Join* join_tables(Storage* storage, OpenedTable* table1, OpenedTable* table2, char* field);
RequestIteratorResult join_next_row(Join* join);
void join_free(Join* join);

#endif //LAB1_JOIN_H
