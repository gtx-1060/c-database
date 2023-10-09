//
// Created by vlad on 09.10.23.
//

#ifndef LAB1_TABLE_H
#define LAB1_TABLE_H

#include "defs.h"

#define TABLE_NAME_MAX_SZ 32
#define FIELD_NAME_MAX_SZ 32
#define TABLE_MIN_ROWS_PER_PAGE 10

typedef enum TableDataType {
    TABLE_FTYPE_INT_32 = 1,
    TABLE_FTYPE_FLOAT = 2,
    TABLE_FTYPE_BOOL = 3,
    TABLE_FTYPE_STRING = 4
} TableDataType;

typedef struct TableSchemeField {
    char name[FIELD_NAME_MAX_SZ];
    TableDataType type;
    uint8_t nullable;
} TableSchemeField;

typedef struct TableScheme {
    uint16_t capacity;
    uint16_t size;
    TableSchemeField* fields;
} TableScheme;

typedef struct TableMeta {
    char name[TABLE_NAME_MAX_SZ];
    TableSchemeField* fields;
    uint16_t page_scale;
    uint32_t row_size;
    uint16_t fields_n;
    uint32_t scheme_page;
    uint32_t data_page;
} TableMeta;

#endif //LAB1_TABLE_H
