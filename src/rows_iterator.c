//
// Created by vlad on 21.11.23.
//

#include "rows_iterator.h"
#include <malloc.h>
#include "string.h"
#include "util.h"

void init_outer_iterator(RowsIterator* iter);
void** current_row(RowsIterator* iter);

RowsIterator* init(Storage* storage, const OpenedTable* table) {
    RowsIterator* req_iter = malloc(sizeof(RowsIterator));
    req_iter->row = NULL;
    req_iter->outer = NULL;
    req_iter->filters_list = NULL;

    req_iter->table = table;
    req_iter->storage = storage;

    req_iter->page_pointer = table->mapped_addr->first_free_pg;
    req_iter->row_size = table->mapped_addr->fields_n;
    req_iter->row_offset = 0;
    req_iter->row_pointer = 0;
    req_iter->source = TABLE_FREE_LIST;
    return req_iter;
}

// one iterator cant have multiple inner iterators!!
RowsIterator* create_rows_iterator_inner(Storage* storage, const OpenedTable* table, RowsIterator* outer_iter) {
    RowsIterator* iter = init(storage, table);
    iter->outer = outer_iter;
    init_outer_iterator(iter);
    return iter;
}

void init_outer_iterator(RowsIterator* iter) {
    // if outer scope iterator has nothing to iterate through
    if (rows_iterator_next(iter->outer) == REQUEST_SEARCH_END) {
        iter->source = TABLE_FULL_LIST;
        iter->page_pointer = 0;
    } else {
        iter->row_offset = iter->outer->row_size;
        iter->row_size += iter->row_offset;
        iter->row = realloc(iter->outer->row, iter->row_size);
        if (iter->row == NULL)
            panic("Cannot realloc row (for join purpose)!", 5);
        iter->outer->row = iter->row;
        memset(current_row(iter), 0, iter->table->mapped_addr->fields_n);
    }
}

void** current_row(RowsIterator* iter) {
    return (iter->row + iter->row_offset);
}

RowsIterator* create_rows_iterator(Storage* storage, const OpenedTable* table) {
    RowsIterator* iter = init(storage, table);
    iter->row = malloc(sizeof(void*) * iter->row_size);
    memset(iter->row, 0, sizeof(void*) * iter->row_size);
    return iter;
}

void rows_iterator_add_filter(RowsIterator* iter, filter_predicate predicate, void* value, char* fname) {
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

/*
 * Always extend row with values to compare
 * Pass into the filter ext_row + ind1 + ind2
 */
uint8_t check_filters(RowsIterator* iter) {
    if (iter->filters_list == NULL)
        return 1;
    RequestFilter* f = iter->filters_list;
    do {
        if (!f->predicate(f->field, iter->row, f->value))
            return 0;
        f = (RequestFilter*)f->list.next;
    } while (f != iter->filters_list);
    return 1;
}

RowsIteratorResult rows_iterator_next(RowsIterator* iter) {
    if (iter == NULL)
        return REQUEST_SEARCH_END;
    free_row_content(iter->table->mapped_addr->fields_n, current_row(iter));
    while (iter->source == TABLE_FREE_LIST || iter->page_pointer != 0) {
        RowReadStatus s = table_get_row_in_buff(iter->storage, iter->table, current_row(iter),
                                                iter->page_pointer, iter->row_pointer);
        switch (s) {
            case READ_ROW_OK:
                iter->row_pointer++;
                if (check_filters(iter))
                    return REQUEST_ROW_FOUND;
                break;
            case READ_ROW_IS_NULL:
                iter->row_pointer++;
                break;
            case READ_ROW_OUT_OF_BOUND:
                iter->page_pointer = next_page_index(&iter->storage->manager, iter->page_pointer);
                iter->row_pointer = 0;
                if (iter->page_pointer == 0) {
                    if (iter->source == TABLE_FREE_LIST) {
                        iter->source = TABLE_FULL_LIST;
                        iter->page_pointer = iter->table->mapped_addr->first_full_pg;
                    } else if (iter->outer != NULL && rows_iterator_next(iter->outer) == REQUEST_ROW_FOUND) {
                        iter->page_pointer = iter->table->mapped_addr->first_free_pg;
                        iter->source = TABLE_FREE_LIST;
                    }
                }
                break;
            case READ_ROW_DEST_NOT_FREE:
                panic("DESTINATION FOR THE ROW ARENT FREE", 5);
        }
    }
    return REQUEST_SEARCH_END;
}

void rows_iterator_remove_current(RowsIterator* iter) {
    if (iter->page_pointer == 0 || iter->row_pointer == 0) {
        panic("ATTEMPT TO REMOVE ROW OF [0] PAGE WITH ITERATOR", 5);
    }
    table_remove_row(iter->storage, iter->table, iter->page_pointer, iter->row_pointer-1);
    if (iter->source == TABLE_FULL_LIST) {
        iter->source = TABLE_FREE_LIST;
    }
}

// you can pass NULL in the row, so that field keep its value
void rows_iterator_update_current(RowsIterator* iter, void** row) {
    if (iter->page_pointer == 0) {
        panic("ATTEMPT TO REPLACE ROW OF [0] PAGE WITH ITERATOR", 5);
    }
    for (uint32_t i = 0; i < iter->table->mapped_addr->fields_n; i++) {
        if (row[i] == NULL) {
            row[i] = current_row(iter)[i];
        }
    }
    table_replace_row(iter->storage, iter->table, iter->page_pointer, iter->row_pointer-1,
                      current_row(iter));
}


void rows_iterator_free(RowsIterator* iter) {
    if (iter->row) {
        free_row_content(iter->table->mapped_addr->fields_n, current_row(iter));
        if (iter->outer == NULL)
            free(iter->row);
    }
    while (iter->filters_list && iter->filters_list->list.next != &iter->filters_list->list) {
        void* deleted = lst_pop(&iter->filters_list->list);
        free(deleted);
    }
    if (iter->filters_list != NULL)
        free(iter->filters_list);
    free(iter);
}



