//
// Created by vlad on 24.11.23.
//

#ifndef LAB1_SEARCH_FILTERS_H
#define LAB1_SEARCH_FILTERS_H

#include "rows_iterator.h"

uint8_t equals_filter(SchemeItem* field, void** row, void* value);
uint8_t greater_filter(SchemeItem* field, void** row, void* value);
uint8_t less_filter(SchemeItem* field, void** row, void* value);
uint8_t random_filter(SchemeItem* field, void** row, void* value);

#endif //LAB1_SEARCH_FILTERS_H
