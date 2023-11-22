//
// Created by vlad on 09.10.23.
//

#ifndef LAB1_TABLE_H
#define LAB1_TABLE_H

#include "defs.h"

#define TABLE_NAME_MAX_SZ 32
//#define FIELD_NAME_MAX_SZ 32
#define TABLE_MIN_ROWS_PER_PAGE 10

typedef enum TableDatatype {
    TABLE_FTYPE_INT_32 = 1,
    TABLE_FTYPE_FLOAT = 2,
    TABLE_FTYPE_BOOL = 3,
    TABLE_FTYPE_STRING = 4,
    TABLE_FTYPE_BYTE = 5,
    TABLE_FTYPE_FOREIGN_KEY = 6,
    TABLE_FTYPE_CHARS = 7       // sz = byte for str_length + str_length
} TableDatatype;

typedef struct SchemeItem {
    char* name;
    TableDatatype type;
    uint8_t actual_size;
    uint8_t nullable;
} SchemeItem;

typedef struct TableScheme {
    uint16_t capacity;
    uint16_t size;
    SchemeItem* fields;
} TableScheme;

typedef struct Table {
    char* name;
    uint16_t fields_n;
    uint16_t page_scale;
    uint32_t row_size;
    uint32_t scheme_page;
    SchemeItem* fields;
} Table;

// struct for serialization/deserialization
typedef struct __attribute__((__packed__)) TableRecord {
    char name[TABLE_NAME_MAX_SZ];
    uint16_t fields_n;
    uint16_t page_scale;
    uint32_t row_size;
    uint32_t scheme_page;
    uint32_t first_free_pg;
    uint32_t first_full_pg;
} TableRecord;


#endif //LAB1_TABLE_H
