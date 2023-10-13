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
    PageOnDrive page;
    page = *((PageOnDrive*)pointer);
    dest->scale = page.scale;
    dest->row_size = page.row_size;
    dest->next = page.next;
}

// change the next filed in the header and return the old one
uint32_t change_next_field(MemoryManager* manager, uint32_t offset, uint32_t next_val) {
    uint8_t* pointer = (uint8_t*)get_mapped_pages(manager, offset, 1);
    uint32_t old_next = *(pointer+6);
    *(pointer+6) = next_val;
    return old_next;
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
    uint8_t* mem = (uint8_t*)get_mapped_pages(manager, header->offset, header->scale);
    PageOnDrive page = {.next=header->next, .row_size=header->row_size, .scale=header->scale};
    *((PageOnDrive*)mem) = page;
    uint16_t bytes_for_bitmap = ceil(((double)rows_number(header))/8);
    // 0+4+2+4
    if (memset(mem + sizeof(PageOnDrive), 0, bytes_for_bitmap) == 0)
        return 0; // ERROR
    return header->offset + header->scale*SYS_PAGE_SIZE;
}

// return -1, if the bit already has same value
// return 0 if success
int8_t set_into_bitmap(uint8_t* page, uint32_t index, uint8_t value) {
    uint8_t* bitmap_pointer = page + sizeof(PageOnDrive) + (index/8);
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

int64_t find_empty_row(MemoryManager* manager, const PageMeta* header); // TODO: move it to somewhere

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
    if (find_empty_row(manager, header) == -1)                                          // TODO: may be optimised
        return WRITE_ROW_OK_BUT_FULL;
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

// return -1 if not found
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

RowWriteResult find_and_write_row(MemoryManager* manager, const PageMeta* header, PageRow* row) {
    uint64_t row_id = find_empty_row(manager, header);
    if (row_id == -1)
        return WRITE_ROW_OUT_OF_BOUND;
    return write_row(manager, header, row);
}

