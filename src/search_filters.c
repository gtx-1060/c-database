//
// Created by vlad on 24.11.23.
//

#include <memory.h>
#include "search_filters.h"
#include "util.h"

void rows_iterator_add_filter(RowsIterator* iter, filter_predicate predicate, void* value, char* column) {
    FilterNode* current_filter = rows_iterator_get_filter(iter);
    FilterNode* new_filter = create_filter_node_l(iter, predicate, column, value);
    if (!current_filter) {
        rows_iterator_set_filter(iter, new_filter);
        return;
    }
    rows_iterator_set_filter(
            iter,
            create_filter_and(current_filter, new_filter)
    );
}

FilterNode* create_filter_node_l(RowsIterator* iter, filter_predicate predicate, char* column, void* value) {
    FilterLeaf* node = malloc(sizeof(FilterLeaf));
    node->type = FILTER_LITERAL;
    node->ind1 = find_column_by_name(iter, column, &node->datatype);
    node->value = value;
    node->predicate = predicate;
    return (FilterNode*)node;
}

FilterNode* create_filter_node_v(RowsIterator* iter, filter_predicate predicate, char* column1, char* column2) {
    FilterLeaf* node = malloc(sizeof(FilterLeaf));
    node->type = FILTER_VARIABLE;
    node->ind1 = find_column_by_name(iter, column1, &node->datatype);
    TableDatatype type2;
    node->ind2 = find_column_by_name(iter, column2, &type2);
    // if type of columns are different
    if (node->datatype != type2) {
        free(node);
        return NULL;
    }
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
int compare_field_value(TableDatatype type, void* value1, void* value2) {
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
}

uint8_t equals_filter(TableDatatype type, void* value1, void* value2) {
    return compare_field_value(type, value1, value2) == 0;
}

uint8_t random_filter(TableDatatype type, void* value1, void* value2) {
    float r = (float)rand()/(float)RAND_MAX;
//    printf("rand = %f, %f\n", r, *(float*)value);
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
    return strstr(value2, value1) != 0;
}

uint8_t in_string(TableDatatype type, void* value1, void* value2) {
    return strstr(value1, value2) != 0;
}