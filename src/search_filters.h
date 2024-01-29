//
// Created by vlad on 24.11.23.
//

#ifndef LAB1_SEARCH_FILTERS_H
#define LAB1_SEARCH_FILTERS_H

#include "rows_iterator.h"


void rows_iterator_add_filter(RowsIterator* iter, filter_predicate predicate, void* value, char* column);

FilterNode* create_filter_node_l(RowsIterator* iter, filter_predicate predicate, char* column, void* value,
                                 uint8_t switch_order, uint8_t values_allocd);
FilterNode* create_filter_node_v(RowsIterator* iter1, RowsIterator* iter2, filter_predicate predicate,
                                 char* column1, char* column2, uint8_t values_allocd);
FilterNode* create_filter_node_ll(filter_predicate predicate, TableDatatype datatype, void* value1, void* value2);

FilterNode* create_filter_and(FilterNode* l, FilterNode* r);
FilterNode* create_filter_or(FilterNode* l, FilterNode* r);

uint8_t equals_filter(TableDatatype type, void* value1, void* value2);
uint8_t not_equals_filter(TableDatatype type, void* value1, void* value2);
uint8_t greater_filter(TableDatatype type, void* value1, void* value2);
uint8_t greater_eq_filter(TableDatatype type, void* value1, void* value2);
uint8_t less_filter(TableDatatype type, void* value1, void* value2);
uint8_t less_eq_filter(TableDatatype type, void* value1, void* value2);
uint8_t substring_of(TableDatatype type, void* value1, void* value2);
uint8_t in_string(TableDatatype type, void* value1, void* value2);

uint8_t random_filter(TableDatatype type, void* value1, void* value2);

#endif //LAB1_SEARCH_FILTERS_H
