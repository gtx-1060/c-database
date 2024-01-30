//
// Created by vlad on 10.01.24.
//

#ifndef LAB3_SCHEME_H
#define LAB3_SCHEME_H

#include <stddef.h>
#include <string.h>
#include <stdint-gcc.h>
#include "util.h"

typedef enum TableDatatype {
    TABLE_FTYPE_INT_32 = 1,
    TABLE_FTYPE_FLOAT = 2,
    TABLE_FTYPE_BOOL = 3,
    TABLE_FTYPE_STRING = 4,
    TABLE_FTYPE_BYTE = 5,
//    TABLE_FTYPE_FOREIGN_KEY = 6,
    TABLE_FTYPE_CHARS = 7,
    TABLE_FTYPE_UINT_32 = 8,
    TABLE_FTYPE_UINT_16 = 9,
} TableDatatype;

typedef struct SchemeItem {
    char* name;
    TableDatatype type;
    uint16_t order;
    uint8_t nullable;
    uint16_t max_sz;
} SchemeItem;

typedef struct TableScheme {
    uint16_t capacity;
    uint16_t size;
    SchemeItem* fields;
} TableScheme;

TableScheme create_table_scheme(uint16_t fields_n);
void add_scheme_field(TableScheme* scheme, const char* name, TableDatatype type, uint8_t nullable);
void insert_scheme_field(TableScheme* scheme, const char* name, TableDatatype type,
                         uint8_t nullable, uint16_t index);
void set_last_field_size(TableScheme* scheme, uint16_t actual_size);
void order_scheme_items(SchemeItem* array, size_t len);
void free_scheme(SchemeItem* scheme, size_t length);

#endif //LAB3_SCHEME_H
