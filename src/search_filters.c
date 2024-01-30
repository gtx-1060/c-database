//
// Created by vlad on 24.11.23.
//

#include <memory.h>
#include "search_filters.h"
#include "util.h"

void rows_iterator_add_filter(RowsIterator* iter, filter_predicate predicate, void* value, char* column) {
    FilterNode* current_filter = rows_iterator_get_filter(iter);
    FilterNode* new_filter = create_filter_node_l(iter, predicate, column, value, 0, 0);
    if (!current_filter) {
        rows_iterator_set_filter(iter, new_filter);
        return;
    }
    rows_iterator_set_filter(
            iter,
            create_filter_and(current_filter, new_filter)
    );
}

// filter for column-literal conditions
FilterNode* create_filter_node_l(RowsIterator* iter, filter_predicate predicate, char* column, void* value,
                                 uint8_t switch_order, uint8_t values_allocd) {
    FilterLeaf* node = malloc(sizeof(FilterLeaf));
    node->type = FILTER_LITERAL;
    int ind = iterator_find_column_index(iter, column, &node->datatype);
    if (ind < 0) {
        free(node);
        return 0;
    }
    node->ind1 = (uint16_t)ind;
    node->value = value;
    node->predicate = predicate;
    node->switch_order = switch_order;
    node->values_allocd = values_allocd;
    return (FilterNode*)node;
}

// filter for predefined literal-literal conditions
FilterNode* create_filter_node_ll(filter_predicate predicate, TableDatatype datatype,
                                  void* value1, void* value2) {
    PredefinedFilterLeaf* leaf = malloc(sizeof(PredefinedFilterLeaf));
    if (predicate(datatype, value1, value2)) {
        *leaf = FILTER_ALWAYS_TRUE;
    } else {
        *leaf = FILTER_ALWAYS_FALSE;
    }
    return (FilterNode*)leaf;
}

// filter for column-column conditions
FilterNode* create_filter_node_v(RowsIterator* iter1, RowsIterator* iter2, filter_predicate predicate,
                                 char* column1, char* column2, uint8_t values_allocd) {
    FilterLeaf* node = malloc(sizeof(FilterLeaf));
    node->type = FILTER_VARIABLE;
    int ind = iterator_find_column_index(iter1, column1, &node->datatype);
    if (ind < 0) {
        free(node);
        return 0;
    }
    node->ind1 = ind;
    TableDatatype type2;
    ind = iterator_find_column_index(iter2, column2, &type2);
    if (ind < 0) {
        free(node);
        return 0;
    }
    node->ind2 = ind;
    // if type of columns are different
    if (node->datatype != type2) {
        free(node);
        return 0;
    }
    node->values_allocd = values_allocd;
    node->predicate = predicate;
    return (FilterNode*)node;
}

FilterNode* create_filter_and(FilterNode* l, FilterNode* r) {
    FilterNode* node = malloc(sizeof(FilterNode));
    node->type = FILTER_AND_NODE;
    node->l = l;
    node->r = r;
    return node;
}

FilterNode* create_filter_or(FilterNode* l, FilterNode* r) {
    FilterNode* node = malloc(sizeof(FilterNode));
    node->type = FILTER_OR_NODE;
    node->l = l;
    node->r = r;
    return node;
}

// -1 means value2 < row[ind]
// 0 means eq
// 1 means value2 > row[ind]
static int compare_field_value(TableDatatype type, void* value1, void* value2) {
    switch (type) {
        case TABLE_FTYPE_INT_32:
            if (*(int32_t*)value2 < *(int32_t*)value1)
                return -1;
            return (*(int32_t*)value2 > *(int32_t*)value1);
        case TABLE_FTYPE_FLOAT:
            if (*(float*)value2 < *(float*)value1)
                return -1;
            return (*(float*)value2 > *(float*)value1);
        case TABLE_FTYPE_BOOL:
            if (*(uint8_t*)value2 < *(uint8_t*)value1)
                return -1;
            return (*(uint8_t*)value2 > *(uint8_t*)value1);
        case TABLE_FTYPE_BYTE:
            if (*(int8_t*)value2 < *(int8_t*)value1)
                return -1;
            return (*(int8_t*)value2 > *(int8_t*)value1);
        case TABLE_FTYPE_UINT_32:
            if (*(uint32_t*)value2 < *(uint32_t*)value1)
                return -1;
            return (*(uint32_t*)value2 > *(uint32_t*)value1);
        case TABLE_FTYPE_UINT_16:
            if (*(uint16_t*)value2 < *(uint16_t*)value1)
                return -1;
            return (*(uint16_t*)value2 > *(uint16_t*)value1);
        case TABLE_FTYPE_STRING:
        case TABLE_FTYPE_CHARS:                             // may occur an exception! because of non-bound cmp
            return strcmp((char*)value1, (char*)value2);
        default:
            panic("CANT COMPARE THIS TYPE", 6);
    }
    // just for sanitizers
    return 0;
}

uint8_t equals_filter(TableDatatype type, void* value1, void* value2) {
    return compare_field_value(type, value1, value2) == 0;
}

uint8_t not_equals_filter(TableDatatype type, void* value1, void* value2) {
    return compare_field_value(type, value1, value2) != 0;
}

// just random predicate for stress testing purpose
uint8_t random_filter(__attribute__((unused)) TableDatatype type, void* value1, __attribute__((unused)) void* value2) {
    float r = (float)rand()/(float)RAND_MAX;
    return r < *(float*)value1;
}


uint8_t greater_filter(TableDatatype type, void* value1, void* value2) {
    return compare_field_value(type, value1, value2) == -1;
}

uint8_t less_filter(TableDatatype type, void* value1, void* value2) {
    return compare_field_value(type, value1, value2) == 1;
}

uint8_t greater_eq_filter(TableDatatype type, void* value1, void* value2) {
    return compare_field_value(type, value1, value2) <= 0;
}

uint8_t less_eq_filter(TableDatatype type, void* value1, void* value2) {
    return compare_field_value(type, value1, value2) >= 0;
}

uint8_t substring_of(TableDatatype type, void* value1, void* value2) {
    if (type != TABLE_FTYPE_STRING && type != TABLE_FTYPE_CHARS) {
        panic("wrong type", 6);
    }
    return strstr(value2, value1) != 0;
}

uint8_t in_string(TableDatatype type, void* value1, void* value2) {
    if (type != TABLE_FTYPE_STRING && type != TABLE_FTYPE_CHARS) {
        panic("wrong type", 6);
    }
    return strstr(value1, value2) != 0;
}
