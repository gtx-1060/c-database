//
// Created by vlad on 12.10.23.
//

#ifndef LAB1_STORAGE_H
#define LAB1_STORAGE_H

#include "mem_mapping.h"
#include "table.h"
#include "page.h"

#define RESERVED_TO_FILE_META 2
//#define RESERVED_TO_TABLES 4
#define FILE_HEADER_MAGIC_NUMBER 0x7F38
#define ONE_TABLE_META_ON_DRIVE_SIZE 52

typedef struct FileHeader {
    // ALL THE VALUES INCLUDE POINTERS TO THE MAPPED
    // MEMORY THAT COULD BE NOT ALIVE
    uint16_t magic_number;
    uint16_t data_offset;
    uint16_t tables_number;
    uint32_t pages_number;
} FileHeader;

typedef struct LoadedTable {
    Table* table_meta;
    Chunk* chunk;
    TableRecord* mapped_addr;
} LoadedTable;

typedef struct Storage {
    MemoryManager manager;
    Chunk* header_chunk;
} Storage;

typedef struct FieldValue {
    char* field_name;
    void* value;
} FieldValue;

typedef struct GotTableRow {
    void** data;
    RowReadResult result;
} GotTableRow;

// add table entity into the *tables* table
// add scheme into the *schemes* table
LoadedTable create_table(Storage* storage, Table* table);

// loads information about the table
// permanently maps it into memory
LoadedTable open_table(const Storage* storage, char* name);

//RequestFilterResult table_filter_rows(const Storage* manager, Table* table, FieldValue filter);

//uint8_t request_filter_next(RequestFilterResult* iter);
//
//PageRow request_filter_get_row(RequestFilterResult* iter);
//
//PageMeta request_filter_get_page(RequestFilterResult* iter);
//
//void** request_filter_get_record(RequestFilterResult* iter);

void table_insert_row(Storage* storage, LoadedTable* table, void* data[]);
void table_remove_row(Storage* storage, LoadedTable* table, uint32_t page_ind, uint32_t row_ind);
GotTableRow table_get_row(Storage* storage, LoadedTable* table, uint32_t page_ind, uint32_t row_ind);

void* flatten_fields_array(LoadedTable* table, void** array);
void free_row_mem(LoadedTable* table, void** row);

//Table* get_all_tables(const MemoryManager* memory, uint16_t* number);

//void table_remove_record(const MemoryManager* memory, Table* table, FieldValue filter);



#endif //LAB1_STORAGE_H
