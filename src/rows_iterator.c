//
// Created by vlad on 21.11.23.
//

#include "rows_iterator.h"
#include "string.h"
#include "util.h"

void init_outer_iterator(RowsIterator* iter);
void** current_row(RowsIterator* iter);

RowsIterator* init(Storage* storage, const OpenedTable* table) {
    RowsIterator* req_iter = malloc(sizeof(RowsIterator));
    req_iter->row = NULL;
    req_iter->outer = NULL;
    req_iter->filters = NULL;

    req_iter->table = table;
    req_iter->storage = storage;

    req_iter->page_pointer = table->mapped_addr->first_free_pg;
    req_iter->row_size = table->mapped_addr->fields_n;
    req_iter->row_offset = 0;
    req_iter->row_pointer = 0;
    req_iter->source = TABLE_FREE_LIST;
    return req_iter;
}

// one iterator cant have multiple inner iterators at time!!
RowsIterator* create_rows_iterator_inner(const OpenedTable* table, RowsIterator* outer_iter) {
    RowsIterator* iter = init(outer_iter->storage, table);
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
        iter->row = realloc(iter->outer->row, sizeof(void*) * iter->row_size);
        if (iter->row == NULL)
            panic("Cannot realloc row (for join purpose)!", 5);
        iter->outer->row = iter->row;
        memset(current_row(iter), 0, sizeof(void*)*iter->table->mapped_addr->fields_n);
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

FilterNode* rows_iterator_get_filter(RowsIterator* iter) {
    return iter->filters;
}

void rows_iterator_set_filter(RowsIterator* iter, FilterNode* filter) {
    iter->filters = filter;
}

// return index of column and its type
// or -1 if not found
int iterator_find_column_index(RowsIterator* iter, char* column, TableDatatype* type) {
    int index = table_find_index_of_column(iter->table, column);
    if (index < 0)
        return -1;
    *type = iter->table->scheme[index].type;
    return iter->row_offset + index;
}

/*
 * Always extend row with values f outer scope iterator
 * Pass into the filter ext_row + ind1 + ind2 if variable comparison
 */
uint8_t check_filters(FilterNode* filter, void* row[]) {
    if (!filter)
        return 1;
    switch (filter->type) {
        case FILTER_AND_NODE:
            return check_filters(filter->l, row) && check_filters(filter->r, row);
        case FILTER_OR_NODE:
            return check_filters(filter->l, row) || check_filters(filter->r, row);
        case FILTER_LITERAL: {
            FilterLeaf* leaf = (FilterLeaf*)filter;
            if (leaf->switch_order)
                return leaf->predicate(leaf->datatype, leaf->value, row[leaf->ind1]);
            return leaf->predicate(leaf->datatype, row[leaf->ind1], leaf->value);
        }
        case FILTER_VARIABLE: {
            FilterLeaf* leaf = (FilterLeaf*)filter;
            return leaf->predicate(leaf->datatype, row[leaf->ind1], row[leaf->ind2]);
        }
        case FILTER_ALWAYS_FALSE:
            return 0;
        case FILTER_ALWAYS_TRUE:
            return 1;
    }
    panic("cant parse iterator filters", 5);
    return 0;
}

RowsIteratorResult rows_iterator_next(RowsIterator* iter) {
    if (iter == NULL)
        return REQUEST_SEARCH_END;
    while (iter->source == TABLE_FREE_LIST || iter->page_pointer != 0) {
        free_row_content(iter->table->mapped_addr->fields_n, current_row(iter));
        RowReadStatus s = table_get_row_in_buff(iter->storage, iter->table, current_row(iter),
                                                iter->page_pointer, iter->row_pointer);
        switch (s) {
            case READ_ROW_OK:
                iter->row_pointer++;
                if (check_filters(iter->filters, iter->row))
                    return REQUEST_ROW_FOUND;
                break;
            case READ_ROW_IS_NULL:
                iter->row_pointer++;
                break;
            case READ_ROW_OUT_OF_BOUND:
                iter->page_pointer = next_page_index(&iter->storage->manager, iter->page_pointer);
                iter->row_pointer = 0;
                if (iter->page_pointer == 0) {
                    if (iter->source == TABLE_FREE_LIST && iter->table->mapped_addr->first_full_pg) {
                        iter->source = TABLE_FULL_LIST;
                        iter->page_pointer = iter->table->mapped_addr->first_full_pg;
                    } else if (iter->outer && rows_iterator_next(iter->outer) == REQUEST_ROW_FOUND) {
                        iter->page_pointer = iter->table->mapped_addr->first_free_pg;
                        iter->source = TABLE_FREE_LIST;
                    } else {
                        return REQUEST_SEARCH_END;
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
                      row);
}

void free_filters(FilterNode* filter) {
    if (!filter)
        return;
    if (filter->type == FILTER_AND_NODE || filter->type == FILTER_OR_NODE) {
        free_filters(filter->l);
        free_filters(filter->r);
    } else if (filter->type == FILTER_LITERAL && ((FilterLeaf*)filter)->values_allocd) {
        free(((FilterLeaf*)filter)->value);
    }
    free(filter);
}

void rows_iterator_free(RowsIterator* iter) {
    if (iter->row) {
        free_row_content(iter->table->mapped_addr->fields_n, current_row(iter));
        if (iter->outer == NULL)
            free(iter->row);
    }
    free_filters(iter->filters);
    free(iter);
}



