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
//    destroy_memory_manager(manager->memory_manager);
//    free(manager);
//}

// WARNING: when you call it twice or call get_pages()
// the first mapped page would destroy itself
PageRecord* map_page_header(MemoryManager* manager, uint32_t offset) {
    return (PageRecord*)get_pages(manager, offset, 1);
}

void read_page_meta(MemoryManager* manager, uint32_t offset, PageMeta* dest) {
    dest->offset = offset;
    PageRecord* pointer = map_page_header(manager, offset);
    if (pointer->row_size == 0) {
        panic("ATTEMPT TO READ NON PAGE MEMORY!", 2);
    }
    dest->scale = pointer->scale;
    dest->row_size = pointer->row_size;
    dest->next = pointer->next;
}

void write_page_meta(MemoryManager* manager, PageMeta* header) {
    PageRecord* pointer = map_page_header(manager,  header->offset);
    PageRecord page = {
            .row_size = header->row_size,
            .next = header->next,
            .prev = header->prev,
            .scale = header->scale
    };
    *pointer = page;
}

// maybe it works, maybe not
uint32_t rows_number(const PageMeta* header) {
    return (uint32_t)floor((32768*(double)header->scale-80)/(8*(double)header->row_size+1));
}

uint32_t calc_header_size(uint32_t row_size) {
    return sizeof(PageRecord) + (uint32_t)ceil(((double)row_size) / 8);
}

// = page_size - page_header_size
uint32_t page_actual_size(uint32_t row_size, uint16_t page_scale) {
    return SYS_PAGE_SIZE*page_scale - calc_header_size(row_size);
}

// offset from start of the page
// to the first row of pointer
uint32_t first_row_offset(const PageMeta* header) {
    return calc_header_size(header->offset);
}

// return the offset for the next page
// or zero, if error occurs
uint32_t create_page(MemoryManager* manager, const PageMeta* header) {
    uint8_t* mem = (uint8_t*) get_pages(manager, header->offset, header->scale);
    PageRecord page = {.next=header->next, .row_size=header->row_size, .scale=header->scale};
    *((PageRecord*)mem) = page;
    uint16_t bytes_for_bitmap = ceil(((double)rows_number(header))/8);
    // 0+4+2+4
    if (memset(mem + sizeof(PageRecord), 0, bytes_for_bitmap) == 0)
        return 0; // ERROR
    return header->offset + header->scale*SYS_PAGE_SIZE;
}

// return -1, if the bit already has same value
// return 0 if success
int8_t set_into_bitmap(uint8_t* page, uint32_t index, uint8_t value) {
    uint8_t* bitmap_pointer = page + sizeof(PageRecord) + (index / 8);
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

uint8_t is_row_empty(const uint8_t* page, uint32_t row_index) {
    return (*(page + sizeof(PageRecord) + (row_index / 8)) << (row_index % 8)) & 0b10000000;
}

RowReadResult read_row(MemoryManager* manager, const PageMeta* header, PageRow* dest) {
    uint8_t* page = (uint8_t*) get_pages(manager, header->offset, header->scale);
    if (dest->index >= rows_number(header))
        return READ_ROW_OUT_OF_BOUND;
    if (is_row_empty(page, dest->index))
        return READ_ROW_IS_NULL;
    if (dest->data != NULL)
        return READ_ROW_DEST_NOT_FREE;
    dest->data = malloc(header->row_size);
    if (dest->data == NULL) {
        panic("CANNOT ALLOC MEM FOR ROW", 2);
    }
    memcpy(dest->data, page+first_row_offset(header)+index, header->row_size);
    return READ_ROW_OK;
}

void free_row(PageRow* row) {
    free(row->data);
    row->data = NULL;
}

PageRow alloc_row(uint32_t size, uint32_t index) {
    PageRow row = {.data=malloc(size), .index=index};
    return row;
}

RowWriteResult replace_row(MemoryManager* manager, const PageMeta* header, const PageRow* row) {
    if (row->index >= rows_number(header))
        return WRITE_ROW_OUT_OF_BOUND;
    uint8_t* page = (uint8_t*) get_pages(manager, header->offset, header->scale);
    if(!is_row_empty(page, row->index))
        return WRITE_ROW_NOT_EMPTY;
    memcpy(page+first_row_offset(header)+row->index, row->data, header->row_size);
    return WRITE_ROW_OK;
}

RowWriteResult write_row(MemoryManager* manager, const PageMeta* header, const PageRow* row) {
    if (row->index >= rows_number(header))
        return WRITE_ROW_OUT_OF_BOUND;
    uint8_t* page = (uint8_t*) get_pages(manager, header->offset, header->scale);
    if (!is_row_empty(page, row->index)) {
        return WRITE_ROW_NOT_EMPTY;
    }
    if(set_into_bitmap(page, row->index, 1) != 0 )
        return WRITE_BITMAP_ERROR;
    memcpy(page+first_row_offset(header)+row->index, row->data, header->row_size);
    if (find_empty_row(manager, header) == -1)                                          // TODO: may be optimised
        return WRITE_ROW_OK_BUT_FULL;
    return WRITE_ROW_OK;
}

RowRemoveResult remove_row(MemoryManager* manager, const PageMeta* header, uint32_t row_ind) {
    if (row_ind >= rows_number(header))
        return REMOVE_ROW_OUT_OF_BOUND;
    int64_t empty_row = find_empty_row(manager, header);
    uint8_t* page = (uint8_t*) get_pages(manager, header->offset, header->scale);
    memset(page + first_row_offset(header) + row_ind, 0, header->row_size);
    if(set_into_bitmap(page, row_ind, 0) != 0 )
        return REMOVE_BITMAP_ERROR;
    if (empty_row == -1)
        return REMOVE_ROW_OK_PAGE_FREED;
    return REMOVE_ROW_OK;
}

// return -1 if not found
int64_t find_empty_row(MemoryManager* manager, const PageMeta* header) {
    uint8_t* page = (uint8_t*) get_pages(manager, header->offset, 1);
    uint32_t rows = rows_number(header);
    uint32_t i = 0;
    for (uint8_t* bitmap = page+ sizeof(PageRecord); bitmap < page + sizeof(PageRecord) + (rows / 8); bitmap++) {
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

RowWriteResult find_and_write_row(MemoryManager* manager, const PageMeta* header, void* data) {
    uint64_t row_id = find_empty_row(manager, header);
    if (row_id == -1)
        return WRITE_ROW_OUT_OF_BOUND;
    PageRow row = {.data = data, .index = row_id};
    return write_row(manager, header, &row);
}

uint32_t next_page_index(MemoryManager* manager, uint32_t current_page) {
    return map_page_header(manager, current_page)->next;
}