//
// Created by vlad on 21.11.23.
//

#ifndef LAB1_REQUEST_ITERATOR_H
#define LAB1_REQUEST_ITERATOR_H
#include "defs.h"
#include "storage.h"

typedef uint8_t (*filter_predicate)(SchemeItem* field, void* row[], void* value);

typedef enum RequestIteratorResult {
    REQUEST_ROW_FOUND,
    REQUEST_SEARCH_END
} RequestIteratorResult;


typedef struct RequestFilter {
    List* list;
    filter_predicate predicate;
    void* value;
    SchemeItem* field;
} RequestFilter;

typedef struct RequestIterator {
//    filter_predicate predicate;
    const OpenedTable* table;
    Storage* storage;
//    uint32_t field_ind;             // index of compared field in row
//    void* value;                    // desired value
//    SchemeItem* field;              // compared field
    RequestFilter* filters_list;
    uint32_t page_pointer;
    uint32_t row_pointer;
    void** found;
    enum TableList {TABLE_FREE_LIST, TABLE_FULL_LIST} source;
} RequestIterator;


RequestIterator* create_request_iterator(Storage* storage, const OpenedTable* table);
void request_iterator_add_filter(RequestIterator* iter, filter_predicate predicate, void* value, char* fname);

RequestIteratorResult request_iterator_next(RequestIterator* iter);
void request_iterator_remove_current(RequestIterator* iter);
void request_iterator_replace_current(RequestIterator* iter, void** row);
void** request_iterator_take_found(RequestIterator* iter);
void request_iterator_free(RequestIterator* iter);

#endif //LAB1_REQUEST_ITERATOR_H
