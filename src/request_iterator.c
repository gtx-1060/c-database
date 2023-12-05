//
// Created by vlad on 21.11.23.
//

#include "request_iterator.h"
#include <malloc.h>
#include "string.h"
#include "util.h"

RequestIterator* create_request_iterator(Storage* storage, const OpenedTable* table) {
    RequestIterator* req_iter = malloc(sizeof(RequestIterator));
    req_iter->table = table;
    req_iter->found = NULL;
    req_iter->storage = storage;
    req_iter->filters_list = NULL;
    req_iter->row_pointer = 0;
    req_iter->page_pointer = table->mapped_addr->first_free_pg;
    req_iter->source = TABLE_FREE_LIST;
    return req_iter;
}

void request_iterator_add_filter(RequestIterator* iter, filter_predicate predicate, void* value, char* fname) {
    for (uint32_t i = 0; i < iter->table->mapped_addr->fields_n; i++) {
        if (strcmp(iter->table->scheme[i].name, fname) == 0) {
            RequestFilter* filter = malloc(sizeof(RequestFilter));
            filter->field = iter->table->scheme+i;
            filter->value = value;
            filter->predicate = predicate;
            if (iter->filters_list == NULL) {
                lst_init(&filter->list);
                iter->filters_list = filter;
                return;
            }
            lst_push(&iter->filters_list->list, filter);
            return;
        }
    }
}

uint8_t check_filters(RequestIterator* iter, void* row[]) {
    if (iter->filters_list == NULL)
        return 1;
    RequestFilter* f = iter->filters_list;
    do {
        if (!f->predicate(f->field, row, f->value))
            return 0;
        f = (RequestFilter*)f->list.next;
    } while (f != iter->filters_list);
    return 1;
}

RequestIteratorResult request_iterator_next(RequestIterator* iter) {
    GetRowResult row;
    while (iter->source == TABLE_FREE_LIST || iter->page_pointer != 0) {
        row = table_get_row(iter->storage, iter->table, iter->page_pointer, iter->row_pointer);
        switch (row.result) {
            case READ_ROW_OK:
                iter->row_pointer++;
                if (check_filters(iter, row.data)) {
                    if (iter->found)
                        free_row_array(iter->table->mapped_addr->fields_n, iter->found);
                    iter->found = row.data;
                    return REQUEST_ROW_FOUND;
                } else {
                    free_row_array(iter->table->mapped_addr->fields_n, row.data);
                }
                break;
            case READ_ROW_IS_NULL:
                iter->row_pointer++;
                break;
            case READ_ROW_OUT_OF_BOUND:
                iter->page_pointer = next_page_index(&iter->storage->manager, iter->page_pointer);
                iter->row_pointer = 0;
                if (iter->page_pointer == 0 && iter->source == TABLE_FREE_LIST) {
                    iter->source = TABLE_FULL_LIST;
                    iter->page_pointer = iter->table->mapped_addr->first_full_pg;
                }
                break;
            case READ_ROW_DEST_NOT_FREE:
                panic("DESTINATION FOR THE ROW ARENT FREE", 5);
        }
    }
    return REQUEST_SEARCH_END;
}

void request_iterator_remove_current(RequestIterator* iter) {
    if (iter->page_pointer == 0 || iter->row_pointer == 0) {
        panic("ATTEMPT TO REMOVE ROW OF [0] PAGE WITH ITERATOR", 5);
    }
    table_remove_row(iter->storage, iter->table, iter->page_pointer, iter->row_pointer-1);
    if (iter->source == TABLE_FULL_LIST) {
        iter->source = TABLE_FREE_LIST;
    }
}

// you can pass NULL in the row, so that field keep its value
void request_iterator_replace_current(RequestIterator* iter, void** row) {
    if (iter->page_pointer == 0 || iter->found == NULL) {
        panic("ATTEMPT TO REPLACE ROW OF [0] PAGE WITH ITERATOR", 5);
    }
    for (uint32_t i = 0; i < iter->table->mapped_addr->fields_n; i++) {
        if (row[i] == NULL) {
            row[i] = iter->found[i];
        }
    }
    table_replace_row(iter->storage, iter->table, iter->page_pointer, iter->row_pointer-1, row);
}

// returns list of row scheme delegates responsibility for
// freeing it to the caller
void** request_iterator_take_found(RequestIterator* iter) {
    void** data = iter->found;
    iter->found = NULL;
    return data;
}


void request_iterator_free(RequestIterator* iter) {
    if (iter->found)
        free_row_array(iter->table->mapped_addr->fields_n, iter->found);
    while (iter->filters_list && iter->filters_list->list.next != &iter->filters_list->list) {
        void* deleted = lst_pop(&iter->filters_list->list);
        free((RequestFilter*)deleted);
    }
    if (iter->filters_list != NULL)
        free(iter->filters_list);
    free(iter);
}



