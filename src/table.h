//
// Created by vlad on 09.10.23.
//

#ifndef LAB1_TABLE_H
#define LAB1_TABLE_H

#include "defs.h"
#include <stddef.h>

#define TABLE_NAME_MAX_SZ 32
//#define FIELD_NAME_MAX_SZ 32
#define TABLE_MIN_ROWS_PER_PAGE 10

#define HEAP_TABLES_NAME "#heap_table#"
#define HEAP_ROW_SIZE_STEP 50

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
//    uint16_t actual_sz;     // uses only for TABLE_FTYPE_CHARS
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
    SchemeItem* fields;
} Table;

// struct for serialization/deserialization
typedef struct __attribute__((__packed__))TableRecord {
    char name[TABLE_NAME_MAX_SZ];
    uint16_t table_id;
    uint16_t fields_n;
    uint16_t page_scale;
    uint32_t row_size;
    uint32_t first_free_pg;
    uint32_t first_full_pg;
} TableRecord;

Table* init_table(const TableScheme* scheme, const char* name);

TableScheme create_table_scheme(uint16_t fields_n);
void add_scheme_field(TableScheme* scheme, const char* name, TableDatatype type, uint8_t nullable);
void insert_scheme_field(TableScheme* scheme, const char* name, TableDatatype type,
                         uint8_t nullable, uint16_t index);
void set_last_field_size(TableScheme* scheme, uint16_t actual_size);
void order_scheme_items(SchemeItem* array, size_t len);

void free_scheme(SchemeItem* scheme, size_t length);
void destruct_table(Table* table);

TableScheme get_table_of_tables_scheme();
TableScheme get_scheme_table_scheme();
TableScheme get_heap_table_scheme(uint32_t str_len);
uint16_t get_nearest_heap_size(uint32_t str_len);

#endif //LAB1_TABLE_H
