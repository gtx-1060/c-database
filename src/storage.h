//
// Created by vlad on 12.10.23.
//

#ifndef LAB1_STORAGE_H
#define LAB1_STORAGE_H

#include "mem_mapping.h"
#include "table.h"
#include "page.h"
#include "rows_iterator.h"

#define RESERVED_TO_FILE_META 2
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

typedef struct OpenedTable {
    SchemeItem* scheme;
    UserChunk chunk;
    TableRecord* mapped_addr;
} OpenedTable;

typedef struct Storage {
    MemoryManager manager;
    UserChunk header_chunk;
    OpenedTable tables;
    OpenedTable scheme_table;
    OpenedTable free_page_table;
} Storage;

typedef struct GetRowResult {
    void** data;
    RowReadStatus result;
} GetRowResult;

typedef struct InsertRowResult {
    uint32_t page_id;
    uint32_t row_id;
} InsertRowResult;

// add table entity into the *tables* table
//void write_table(Storage* storage, Table* table, OpenedTable* dest);

typedef struct RowsIterator RowsIterator;

// loads information about the table
// permanently maps it into memory
uint8_t map_table(Storage* storage, RowsIterator* iter, OpenedTable* dest);
void* prepare_row_for_insertion(Storage* storage, const OpenedTable* table, void* array[]);
void write_table(Storage* storage, Table * table, OpenedTable* dest);

InsertRowResult table_insert_row(Storage* storage, const OpenedTable* table, void* array[]);
void table_remove_row(Storage* storage, const OpenedTable* table, uint32_t page_ind, uint32_t row_ind);

RowReadStatus table_get_row_in_buff(Storage* storage, const OpenedTable* table, void* buffer[],
                                    uint32_t page_ind, uint32_t row_ind);
GetRowResult table_get_row(Storage* storage, const OpenedTable* table, uint32_t page_ind, uint32_t row_ind);

void table_replace_row(Storage* storage, const OpenedTable* table, uint32_t page_ind,uint32_t row_ind, void* array[]);

void free_row_content(uint16_t fields, void** row);
FileHeader* get_header(Storage* storage);
PageMeta storage_add_page(Storage* storage, uint16_t scale, uint32_t row_size);

void create_table(Storage* storage, Table* table, OpenedTable* dest);
uint8_t open_table(Storage* storage, char* name, OpenedTable* dest);
void close_table(Storage* storage, OpenedTable* table);

//Table* get_all_tables(const MemoryManager* memory, uint16_t* number);

//void table_remove_record(const MemoryManager* memory, Table* table, FieldValue predicate);



#endif //LAB1_STORAGE_H
