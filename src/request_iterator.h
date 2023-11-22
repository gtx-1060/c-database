//
// Created by vlad on 21.11.23.
//

#ifndef LAB1_REQUEST_ITERATOR_H
#define LAB1_REQUEST_ITERATOR_H
#include "defs.h"
#include "storage.h"

typedef uint8_t (*search_filter)(LoadedTable* table, uint32_t ind, void** row, void* value);

typedef enum RequestIteratorResult {
    REQUEST_ROW_FOUND,
    REQUEST_SEARCH_END
} RequestIteratorResult;

typedef struct RequestIterator {
    search_filter filter;
    LoadedTable* table;
    Storage* storage;
    uint32_t field_ind;
    void* value;
    uint32_t page_pointer;
    uint32_t row_pointer;
    void** found;
    enum TableList {TABLE_FREE_LIST, TABLE_FULL_LIST} source;
} RequestIterator;

#endif //LAB1_REQUEST_ITERATOR_H
