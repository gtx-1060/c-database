//
// Created by vlad on 04.12.23.
//

#include <string.h>
#include <malloc.h>
#include "join.h"
#include "search_filters.h"

uint8_t find_field_in_scheme(OpenedTable* table, char* field, uint16_t* dest) {
    for (uint16_t i = 0; i < table->mapped_addr->fields_n; i++) {
        if (strcmp(table->scheme[i].name, field) == 0) {
            *dest = i;
            return 1;
        }
    }
    return 0;
}

Join* join_tables(Storage* storage, OpenedTable* table1, OpenedTable* table2, char* field) {
    Join* join = malloc(sizeof(Join));
    join->iter1 = create_rows_iterator(storage, table1);
    join->row1_len = table1->mapped_addr->fields_n;
    join->row2_len = table2->mapped_addr->fields_n;
    join->row = malloc(sizeof(void*) * (join->row1_len + join->row2_len));
    join->fname = strdup(field);
    memset(join->row, 0, sizeof(void*) * (join->row1_len + join->row2_len));
    if (find_field_in_scheme(table1, field, &join->field_ind_t1) == 0 ||
            find_field_in_scheme(table2, field, &join->field_ind_t2) == 0) {
        free(join);
        return NULL;
    }
    rows_iterator_next(join->iter1);
    join->iter2 = create_rows_iterator(storage, table2);
    void* looking_for = join->iter1->found[join->field_ind_t1];
    rows_iterator_add_filter(join->iter2, equals_filter, looking_for, field);
    memcpy(join->row, join->iter1->found, join->row1_len * sizeof(void*));
    return join;
}

RowsIteratorResult join_next_row(Join* join) {
    while (rows_iterator_next(join->iter2) == REQUEST_SEARCH_END) {
        if (rows_iterator_next(join->iter1) == REQUEST_SEARCH_END)
            return REQUEST_SEARCH_END;
        memcpy(join->row, join->iter1->found, join->row1_len * sizeof(void*));
        RowsIterator* old_iter = join->iter2;
        join->iter2 = create_rows_iterator(join->iter2->storage, join->iter2->table);
        rows_iterator_free(old_iter);
        void* looking_for = join->iter1->found[join->field_ind_t1];
        rows_iterator_add_filter(join->iter2, equals_filter, looking_for, join->fname);
    }

    void** dest = join->row + join->row1_len;
    memcpy(dest,join->iter2->found, join->row2_len * sizeof(void*));
    return REQUEST_ROW_FOUND;
}

void join_free(Join* join) {
    rows_iterator_free(join->iter1);
    rows_iterator_free(join->iter2);
    free(join->row);
    free(join->fname);
    free(join);
}