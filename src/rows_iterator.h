//
// Created by vlad on 21.11.23.
//

#ifndef LAB1_ROWS_ITERATOR_H
#define LAB1_ROWS_ITERATOR_H
#include "defs.h"
#include "table.h"
#include "linked_list.h"
#include "storage.h"

typedef struct OpenedTable OpenedTable;
typedef struct Storage Storage;
typedef uint8_t (*filter_predicate)(SchemeItem* field, void* row[], void* value);

typedef enum RowsIteratorResult {
    REQUEST_ROW_FOUND,
    REQUEST_SEARCH_END
} RowsIteratorResult;

typedef struct RequestFilter {
    List list;
    filter_predicate predicate;
    void* value;
    SchemeItem* field;
} RequestFilter;

typedef struct RowsIterator {
    const OpenedTable* table;
    Storage* storage;
    RowsIterator* outer;
    RequestFilter* filters_list;
    uint32_t page_pointer;
    uint32_t row_pointer;
    uint16_t row_offset;
    uint16_t row_size;      // in fields number include all outer iterators
    void** row;
    enum TableList {TABLE_FREE_LIST, TABLE_FULL_LIST} source;
} RowsIterator;


RowsIterator* create_rows_iterator(Storage* storage, const OpenedTable* table);
void rows_iterator_add_filter(RowsIterator* iter, filter_predicate predicate, void* value, char* fname);

RowsIteratorResult rows_iterator_next(RowsIterator* iter);
void rows_iterator_remove_current(RowsIterator* iter);
void rows_iterator_update_current(RowsIterator* iter, void** row);
void** rows_iterator_take_found(RowsIterator* iter);
void rows_iterator_free(RowsIterator* iter);

#endif //LAB1_ROWS_ITERATOR_H
