//
// Created by vlad on 09.10.23.
//

#ifndef LAB1_TABLE_H
#define LAB1_TABLE_H

#include "defs.h"
#include <stddef.h>
#include "scheme.h"

#define TABLE_NAME_MAX_SZ 32
//#define FIELD_NAME_MAX_SZ 32
#define TABLE_MIN_ROWS_PER_PAGE 10

#define HEAP_TABLES_NAME "#heap_table#"
#define HEAP_ROW_SIZE_STEP 50

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
void destruct_table(Table* table);

int get_datatype_size(TableDatatype type);

TableScheme get_table_of_tables_scheme();
TableScheme get_scheme_table_scheme();
TableScheme get_heap_table_scheme(uint32_t str_len);
uint16_t get_nearest_heap_size(uint32_t str_len);

#endif //LAB1_TABLE_H
