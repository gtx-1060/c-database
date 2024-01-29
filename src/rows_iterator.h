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
// value1 is literal or table value
typedef uint8_t (*filter_predicate)(TableDatatype type, void* value1, void* value2);

typedef enum RowsIteratorResult {
    REQUEST_ROW_FOUND,
    REQUEST_SEARCH_END
} RowsIteratorResult;

typedef enum RequestFilterType {
    FILTER_AND_NODE,
    FILTER_OR_NODE,
    FILTER_LITERAL,
    FILTER_VARIABLE,
    FILTER_ALWAYS_TRUE,
    FILTER_ALWAYS_FALSE
} RequestFilterType;

typedef RequestFilterType PredefinedFilterLeaf;

typedef struct FilterNode {
    RequestFilterType type;
    struct FilterNode* l;
    struct FilterNode* r;
} FilterNode;

typedef struct FilterLeaf {
    RequestFilterType type;
    TableDatatype datatype;
    filter_predicate predicate;
    uint16_t ind1;
    union {
        uint16_t ind2;
        void* value;
    };
    uint8_t switch_order;
    uint8_t values_allocd;
} FilterLeaf;

typedef struct RowsIterator {
    const OpenedTable* table;
    Storage* storage;
    struct RowsIterator* outer;
    FilterNode* filters;
    uint32_t page_pointer;
    uint32_t row_pointer;
    uint16_t row_offset;
    uint16_t row_size;      // in fields number include all outer iterators
    void** row;
    enum TableList {TABLE_FREE_LIST, TABLE_FULL_LIST} source;
} RowsIterator;


RowsIterator* create_rows_iterator(Storage* storage, const OpenedTable* table);
RowsIterator* create_rows_iterator_inner(const OpenedTable* table, RowsIterator* outer_iter);

int iterator_find_column_index(RowsIterator* iter, char* column, TableDatatype* type);
void rows_iterator_set_filter(RowsIterator* iter, FilterNode* filter);
FilterNode* rows_iterator_get_filter(RowsIterator* iter);

RowsIteratorResult rows_iterator_next(RowsIterator* iter);
void rows_iterator_remove_current(RowsIterator* iter);
void rows_iterator_update_current(RowsIterator* iter, void** row);
void rows_iterator_free(RowsIterator* iter);

#endif //LAB1_ROWS_ITERATOR_H
