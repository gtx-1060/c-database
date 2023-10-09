//
// Created by vlad on 28.09.23.
//

#include <malloc.h>
#include <math.h>
#include <memory.h>
#include "page.h"
#include "mem_mapping.h"
#include "util.h"

//typedef struct PageManager {
//    MemoryManager* memory_manager;
//} PageManager;

// in the file it stores like:
// row_size (4bytes), scale (2bytes), next(4bytes),
// and bitmap of rows occupancy
typedef struct PageMeta {
    uint32_t offset;
    // real file data
    uint32_t row_size;
    uint16_t scale;
    uint32_t next;
} PageMeta;

typedef enum RowReadStatus {
    READ_ROW_OK,
    READ_ROW_OUT_OF_BOUND,
    READ_ROW_IS_NULL,
    DEST_NOT_FREE
} RowReadStatus;

typedef enum RowWriteResult {
    WRITE_ROW_OK,
    WRITE_BITMAP_ERROR,
    WRITE_ROW_NOT_EMPTY,
    WRITE_ROW_OUT_OF_BOUND
} RowWriteResult;

typedef struct PageRow {
    uint32_t index;
    uint32_t size;
    void* data;
} PageRow;

//typedef enum PageOperationResult {
//    PAGE_READ_OK,
//    PAGE_NOT_EXISTS,
//    PAGE_UNREADABLE
//} PageOperationResult;

//PageManager* init_page_manager(int file_descriptor) {
//    PageManager* manager = malloc(sizeof(PageManager));
//    if (manager == NULL)
//        ("Cannot create page manager", 2);
//    manager->memory_manager = init_memory_manager(file_descriptor);
//    return manager;
//}
//
//void destruct_page_manager(PageManager* manager) {
//    destruct_memory_manager(manager->memory_manager);
//    free(manager);
//}

void read_page_meta(MemoryManager* manager, uint32_t offset, PageMeta* dest) {
    dest->offset = offset;
    uint8_t* pointer = (uint8_t*)get_mapped_pages(manager, dest->offset, 1);
    dest->row_size = *((uint32_t*)(pointer));
    // 0+4
    dest->scale = *((uint16_t*)(pointer + 4));
    // 0+4+2
    dest->next = *((uint16_t*)(pointer + 6));
}

// maybe it works, maybe not
uint32_t rows_number(const PageMeta* header) {
    return (uint32_t)floor((32768*(double)header->scale-80)/(8*(double)header->row_size+1));
}

uint32_t first_row_offset(const PageMeta* header) {
    return 10+(uint32_t)ceil(((double)rows_number(header))/8);
}

// return -1, if the bit already has same value
// return 0 if success
int8_t set_into_bitmap(uint8_t* page, uint32_t index, uint8_t value) {
    uint8_t* bitmap_pointer = page + 10 + (index/8);
    uint8_t mask = (uint8_t)pow(2, (7-index) % 8);
    uint8_t masked = mask & (*bitmap_pointer);
    if ((masked != 0 && value != 0) || (masked == 0 && value == 0))
        return -1;
    if (value == 1) {
        *bitmap_pointer = (*bitmap_pointer) | mask;
    } else {
        mask = ~mask;
        *bitmap_pointer = (*bitmap_pointer) | mask;
    }
    return 0;
}

uint8_t is_row_null(const uint8_t* page, uint32_t row_index) {
    return (*(page + 10 + (row_index / 8)) << (row_index % 8)) & 0b10000000;
}

RowReadStatus read_row(MemoryManager* manager, PageMeta* header, uint32_t index, PageRow* dest) {
    uint8_t* page = (uint8_t*)get_mapped_pages(manager, header->offset, header->scale);
    if (index >= rows_number(header))
        return READ_ROW_OUT_OF_BOUND;
    if (is_row_null(page, index))
        return READ_ROW_IS_NULL;
    if (dest->data != NULL)
        return DEST_NOT_FREE;
    dest->index = index;
    dest->size = header->row_size;
    dest->data = malloc(header->row_size);
    if (dest->data == NULL) {
        panic("CANNOT ALLOC MEM FOR ROW", 2);
    }
    memcpy(dest->data, page+first_row_offset(header)+index, dest->size);
    return READ_ROW_OK;
}

void free_row(PageRow* row) {
    free(row->data);
    row->data = NULL;
}

RowWriteResult write_row(MemoryManager* manager, PageMeta* header, PageRow* row) {
    if (row->index >= rows_number(header))
        return WRITE_ROW_OUT_OF_BOUND;
    uint8_t* page = (uint8_t*)get_mapped_pages(manager, header->offset, 1);
    if (!is_row_null(page, row->index)) {
        return WRITE_ROW_NOT_EMPTY;
    }
    if(set_into_bitmap(page, row->index, 1) != 0 )
        return WRITE_BITMAP_ERROR;
    memcpy(page+first_row_offset(header)+row->index, page, row->size);
    return WRITE_ROW_OK;
}

int64_t find_empty_row(MemoryManager* manager, PageMeta* header) {
    uint8_t* page = (uint8_t*)get_mapped_pages(manager, header->offset, 1);
    uint32_t rows = rows_number(header);
    uint32_t i = 0;
    for (uint8_t* bitmap = page+10; bitmap < page+10+(rows/8); bitmap++) {
        uint8_t mask = 0b10000000;
        uint8_t in_byte = 0;
        do {
            if (((*bitmap) & mask) != 0)
                return i+in_byte;
            in_byte++;
        } while ((mask >>= 1) != 0);
        i += 8;
    }
    return -1;
}

