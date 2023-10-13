//
// Created by vlad on 12.10.23.
//

#ifndef LAB1_STORAGE_H
#define LAB1_STORAGE_H

#include "mem_mapping.h"
#include "table.h"
#include "page.h"

#define RESERVED_TO_FILE_META 2
#define RESERVED_TO_TABLES 4
#define FILE_HEADER_MAGIC_NUMBER 0x7F38
#define ONE_TABLE_META_ON_DRIVE_SIZE 52

typedef struct FileHeader {
    // ALL THE VALUES INCLUDE POINTERS TO THE MAPPED
    // MEMORY THAT COULD BE NOT ALIVE
    uint16_t* data_offset;
    uint16_t* tables_number;
    uint32_t* pages_number;
} FileHeader;

typedef struct MappedTableMeta {
    TableMeta* table_meta;
    // !MAPPED! can point to the not alive memory
    uint32_t* const first_free_page;
    uint32_t* const first_full_page;
} MappedTableMeta;

typedef struct Storage {
    MemoryManager* page_manager;
    MemoryManager* header_manager;
    FileHeader header;
} Storage;

typedef struct FieldValue {
    char* field_name;
    void* value;
} FieldValue;

typedef struct RequestFilterResult {
    MemoryManager* manager;
    uint32_t prev_page;
    uint32_t current_page;
    uint32_t current_row;
    FieldValue filter;
} RequestFilterResult;

typedef struct TableRecord {
    TableMeta* table;
    void** data;
} TableRecord;

MappedTableMeta create_table(Storage* storage, TableMeta* table);

MappedTableMeta open_table(const Storage* storage, char* name);

RequestFilterResult table_filter_rows(const MemoryManager* manager, TableMeta* table, FieldValue filter);

uint8_t request_filter_next(RequestFilterResult* iter);

PageRow request_filter_get_row(RequestFilterResult* iter);

PageMeta request_filter_get_page(RequestFilterResult* iter);

TableRecord request_filter_get_record(RequestFilterResult* iter);

void table_insert_record(const MemoryManager* memory, TableMeta* table, void* data[]);

TableMeta* get_all_tables(const MemoryManager* memory, uint16_t* number);

void table_remove_record(const MemoryManager* memory, TableMeta* table, FieldValue filter);

void table_edit_record(const MemoryManager* memory, TableMeta* table, FieldValue filter, uint32_t values_number,
                       FieldValue* values);

#endif //LAB1_STORAGE_H
