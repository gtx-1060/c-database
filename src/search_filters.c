//
// Created by vlad on 24.11.23.
//

#include <memory.h>
#include "search_filters.h"
#include "util.h"

// -1 means value < row[ind]
// 0 means eq
// 1 means value > row[ind]
int compare_field_value(SchemeItem* field, void** row, void* value) {
    switch (field->type) {
        case TABLE_FTYPE_INT_32:
            if (*(int32_t*)value < *(int32_t*)row[field->order])
                return -1;
            return (*(int32_t*)value > *(int32_t*)row[field->order]);
        case TABLE_FTYPE_FLOAT:
            if (*(float*)value < *(float*)row[field->order])
                return -1;
            return (*(float*)value > *(float*)row[field->order]);
        case TABLE_FTYPE_BOOL:
            if (*(uint8_t*)value < *(uint8_t*)row[field->order])
                return -1;
            return (*(uint8_t*)value > *(uint8_t*)row[field->order]);
        case TABLE_FTYPE_BYTE:
            if (*(int8_t*)value < *(int8_t*)row[field->order])
                return -1;
            return (*(int8_t*)value > *(int8_t*)row[field->order]);
        case TABLE_FTYPE_UINT_32:
            if (*(uint32_t*)value < *(uint32_t*)row[field->order])
                return -1;
            return (*(uint32_t*)value > *(uint32_t*)row[field->order]);
        case TABLE_FTYPE_UINT_16:
            if (*(uint16_t*)value < *(uint16_t*)row[field->order])
                return -1;
            return (*(uint16_t*)value > *(uint16_t*)row[field->order]);
        case TABLE_FTYPE_STRING:
            return (strcmp((char*)row[field->order], (char*)value) == 0);
        case TABLE_FTYPE_CHARS:
            return (strncmp((char*)row[field->order], (char*)value, field->actual_size) == 0);
        default:
            panic("CANT COMPARE THIS TYPE", 6);
    }
}

uint8_t equals_filter(SchemeItem* field, void** row, void* value) {
    return compare_field_value(field, row, value) == 0;
}

uint8_t greater_filter(SchemeItem* field, void** row, void* value) {
    return compare_field_value(field, row, value) == -1;
}

uint8_t less_filter(SchemeItem* field, void** row, void* value) {
    return compare_field_value(field, row, value) == 1;
}