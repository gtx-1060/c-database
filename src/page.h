//
// Created by vlad on 28.09.23.
//
#include "defs.h"
#include "mem_mapping.h"

#ifndef LAB1_PAGE_H
#define LAB1_PAGE_H

// in the file it stores like:
// row_size (4bytes), scale (2bytes), next(4bytes), prev
// and bitmap of rows occupancy
typedef struct PageMeta {
    uint32_t offset;
    uint32_t row_size;
    uint16_t scale;
    uint32_t prev;
    uint32_t next;
} PageMeta;

typedef struct __attribute__((__packed__)) PageRecord {
    uint32_t row_size;
    uint16_t scale;
    uint32_t prev;
    uint32_t next;
} PageRecord;


typedef struct PageRow {
    uint32_t index;
//    uint32_t size;
    void* data;
} PageRow;

typedef enum RowReadResult {
    READ_ROW_OK,
    READ_ROW_OUT_OF_BOUND,
    READ_ROW_IS_NULL,
    READ_ROW_DEST_NOT_FREE
} RowReadResult;

typedef enum RowWriteResult {
    WRITE_ROW_OK,
    WRITE_ROW_OK_BUT_FULL,
    WRITE_BITMAP_ERROR,
    WRITE_ROW_NOT_EMPTY,
    WRITE_ROW_OUT_OF_BOUND
} RowWriteResult;

typedef enum RowRemoveResult {
    REMOVE_ROW_OK,
    REMOVE_ROW_OK_PAGE_FREED,
    REMOVE_BITMAP_ERROR,
    REMOVE_ROW_EMPTY,
    REMOVE_ROW_OUT_OF_BOUND
} RowRemoveResult;

uint32_t page_actual_size(uint32_t row_size, uint16_t page_scale);
uint32_t create_page(MemoryManager* manager, const PageMeta* header);
RowWriteResult write_row(MemoryManager* manager, const PageMeta* header, const PageRow* row);
RowWriteResult find_and_write_row(MemoryManager* manager, const PageMeta* header, void* data);
PageRecord* map_page_header(MemoryManager* manager, uint32_t offset);
void read_page_meta(MemoryManager* manager, uint32_t offset, PageMeta* dest);
void write_page_meta(MemoryManager* manager, PageMeta* header);
RowWriteResult replace_row(MemoryManager* manager, const PageMeta* header, const PageRow* row);
int64_t find_empty_row(MemoryManager* manager, const PageMeta* header);
RowReadResult read_row(MemoryManager* manager, const PageMeta* header, PageRow* dest);
RowRemoveResult remove_row(MemoryManager* manager, const PageMeta* header, uint32_t row_ind);
void free_row(PageRow* row);
uint32_t next_page_index(MemoryManager* manager, uint32_t current_page);

#endif //LAB1_PAGE_H
