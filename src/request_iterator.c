//
// Created by vlad on 21.11.23.
//

#include "request_iterator.h"
#include <malloc.h>
#include "string.h"
#include "util.h"

RequestIterator* create_request_iterator(Storage* storage, LoadedTable* table, void* value,
                                         char* fname, search_filter* filter) {
    RequestIterator* req_iter = malloc(sizeof(RequestIterator));
    req_iter->table = table;
    req_iter->value = value;
    req_iter->filter = filter;
    req_iter->field_ind = 0;
    req_iter->found = NULL;
    req_iter->storage = storage;
    for (uint32_t i = 0; i < table->table_meta->fields_n; i++) {
        if (strcmp(table->table_meta->fields[i].name, fname) == 0) {
            req_iter->field_ind = i;
            break;
        }
    }
    if (req_iter->field_ind == 0)
        return NULL;
    req_iter->row_pointer = 0;
    req_iter->page_pointer = table->mapped_addr->first_free_pg;
    req_iter->source = TABLE_FREE_LIST;
    return req_iter;
}

RequestIteratorResult request_iterator_next(RequestIterator* iter) {
    GotTableRow row;
    while (iter->source == TABLE_FREE_LIST || iter->page_pointer != 0) {
        row = table_get_row(iter->storage, iter->table, iter->page_pointer, iter->row_pointer);
        switch (row.result) {
            case READ_ROW_OK:
                iter->row_pointer++;
                if (iter->filter(iter->table, iter->field_ind, row.data, iter->value)) {
                    if (iter->found)
                        free_row_mem(iter->table, iter->found);
                    iter->found = row.data;
                    return REQUEST_ROW_FOUND;
                } else {
                    free_row_mem(iter->table, row.data);
                }
                break;
            case READ_ROW_IS_NULL:
                iter->row_pointer++;
                break;
            case READ_ROW_OUT_OF_BOUND:
                iter->page_pointer = next_page_index(&iter->storage->manager, iter->page_pointer);
                if (iter->page_pointer == 0 && iter->source == TABLE_FREE_LIST) {
                    iter->source = TABLE_FULL_LIST;
                    iter->row_pointer = 0;
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
}

void request_iterator_replace_current(RequestIterator* iter, void** row) {
    if (iter->page_pointer == 0) {
        panic("ATTEMPT TO REPLACE ROW OF [0] PAGE WITH ITERATOR", 5);
    }
    PageMeta pg_header;
    PageRow pg_row = {
            .index=iter->row_pointer-1,
            .data=flatten_fields_array(iter->table, row)
    };
    read_page_meta(&iter->storage->manager, iter->page_pointer, &pg_header);
    RowWriteResult result = replace_row(&iter->storage->manager, &pg_header, &pg_row);
    if (result != WRITE_ROW_OK) {
        panic("ERROR WHILE UPDATING ROW", 5);
    }
    free(pg_row.data);
}

// returns list of row fields
// delegates responsibility for
// freeing it to the caller
void** request_iterator_take_found(RequestIterator* iter) {
    void** data = iter->found;
    iter->found = NULL;
    return data;
}


void request_iterator_free(RequestIterator* iter) {
    if (iter->found)
        free_row_mem(iter->table, iter->found);
    free(iter);
}