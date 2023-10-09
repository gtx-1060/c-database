//
// Created by vlad on 28.09.23.
//

#include <malloc.h>
#include <math.h>
#include <memory.h>
#include "page.h"
#include "mem_mapping.h"
#include "util.h"

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

uint32_t calc_header_size(uint32_t row_size) {
    return 10 + (uint32_t)ceil(((double)row_size)/8);
}

uint32_t page_data_space(uint32_t row_size, uint16_t page_scale) {
    return SYS_PAGE_SIZE*page_scale - calc_header_size(row_size);
}

// offset from start of the page
// to the first row of data
uint32_t first_row_offset(const PageMeta* header) {
    return calc_header_size(header->offset);
}

// return the offset for the next page
// or zero, if error occurs
uint32_t create_page(MemoryManager* manager, const PageMeta* header) {
    uint8_t* page = (uint8_t*)get_mapped_pages(manager, header->offset, header->scale);
    *(page) = header->row_size;
    // 0+4
    *(page+4) = header->scale;
    // 0+4+2
    *(page+6) = header->next;
    uint16_t bytes_for_bitmap = ceil(((double)rows_number(header))/8);
    // 0+4+2+4
    if (memset(page+10, 0, bytes_for_bitmap) == 0)
        return 0; // ERROR
    return header->offset + header->scale*SYS_PAGE_SIZE;
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

RowReadStatus read_row(MemoryManager* manager, const PageMeta* header, uint32_t index, PageRow* dest) {
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

PageRow alloc_row(uint32_t size, uint32_t index) {
    PageRow row = {.data=malloc(size), .index=index, .size=size};
    return row;
}

RowWriteResult write_row(MemoryManager* manager, const PageMeta* header, const PageRow* row) {
    if (row->index >= rows_number(header))
        return WRITE_ROW_OUT_OF_BOUND;
    uint8_t* page = (uint8_t*)get_mapped_pages(manager, header->offset, header->scale);
    if (!is_row_null(page, row->index)) {
        return WRITE_ROW_NOT_EMPTY;
    }
    if(set_into_bitmap(page, row->index, 1) != 0 )
        return WRITE_BITMAP_ERROR;
    memcpy(page+first_row_offset(header)+row->index, page, row->size);
    return WRITE_ROW_OK;
}

RowWriteResult remove_row(MemoryManager* manager, const PageMeta* header, uint32_t index) {
    PageRow empty_row = alloc_row(header->row_size, index);
    if (memset(empty_row.data, 0, empty_row.size) == 0)
        return WRITE_BITMAP_ERROR;
    RowWriteResult result = write_row(manager, header, &empty_row);
    free_row(&empty_row);
    return result;
}

int64_t find_empty_row(MemoryManager* manager, const PageMeta* header) {
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
