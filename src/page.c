//
// Created by vlad on 28.09.23.
//

#include <malloc.h>
#include <memory.h>
#include <math.h>
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

void* get_mapped_page_row(MemoryManager* manager, Chunk* chunk, uint32_t row_ind) {
    PageMeta pg;
    read_page_meta(manager, chunk->offset, &pg);
    return (void*)((char*)chunk->pointer + row_offset(&pg, row_ind));
}

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
    dest->prev = pointer->prev;
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
    // x = (pg_size - header_size - ceil(x/8)) / row_size
    // x * (row_size + 0.125) = pg_size - header_size
    // x = (pg_size - header_size) / (row_size + 0.125)
    return (uint32_t)floor((double)(header->scale*SYS_PAGE_SIZE - sizeof(PageRecord)) / ((double)header->row_size+0.125));
}

uint32_t get_header_size(const PageMeta* header) {
    return sizeof(PageRecord) + (uint32_t)ceil((double)rows_number(header) / 8);
}

// = page_size - page_header_size
uint32_t page_actual_size(uint32_t row_size, uint16_t page_scale) {
    PageMeta header = {.scale=page_scale, .row_size=row_size};
    return SYS_PAGE_SIZE * page_scale - get_header_size(&header);
}

// offset from start of the page
// to the first row of pointer
uint32_t first_row_offset(const PageMeta* header) {
    return get_header_size(header);
}

// offset from start of the page
// to the first row of pointer
uint32_t row_offset(const PageMeta* header, uint32_t row_ind) {
    return first_row_offset(header) + row_ind * header->row_size;
}

// return the offset for the next page
// or zero, if error occurs
uint32_t create_page(MemoryManager* manager, const PageMeta* header) {
    void* mem = get_pages(manager, header->offset, header->scale);
    if (memset(mem, 0, header->scale*SYS_PAGE_SIZE) == 0)
        return 0; // ERROR
    PageRecord page = {.next=header->next, .row_size=header->row_size, .scale=header->scale};
    *((PageRecord*)mem) = page;
//    uint16_t bytes_for_bitmap = ceil(((double)rows_number(header))/8);
    return header->offset + header->scale;
}

int8_t set_into_bitmap(uint8_t* page, uint32_t index, uint8_t value) {
    uint8_t* bitmap_pointer = page + sizeof(PageRecord) + (index / 8);
    uint8_t mask = (uint8_t)pow(2, (7-index) % 8);
    uint8_t masked = mask & (*bitmap_pointer);
    if ((masked != 0 && value != 0) || (masked == 0 && value == 0))
        return 0;
    if (value == 1) {
        *bitmap_pointer = (*bitmap_pointer) | mask;
    } else {
        mask = ~mask;
        *bitmap_pointer = (*bitmap_pointer) & mask;
    }
    return 1;
}

uint8_t is_row_empty(const uint8_t* page, uint32_t row_index) {
    return !((*(page + sizeof(PageRecord) + (row_index / 8)) << (row_index % 8)) & 0b10000000);
}

RowReadStatus read_row(MemoryManager* manager, const PageMeta* header, PageRow* dest) {
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
    memcpy(dest->data, page + row_offset(header, dest->index), header->row_size);
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

RowWriteStatus replace_row(MemoryManager* manager, const PageMeta* header, const PageRow* row) {
    if (row->index >= rows_number(header))
        return WRITE_ROW_OUT_OF_BOUND;
    uint8_t* page = (uint8_t*) get_pages(manager, header->offset, header->scale);
    if(!is_row_empty(page, row->index))
        return WRITE_ROW_NOT_EMPTY;
    memcpy(page+first_row_offset(header)+row->index, row->data, header->row_size);
    return WRITE_ROW_OK;
}

RowWriteStatus write_row(MemoryManager* manager, const PageMeta* header, const PageRow* row) {
    if (row->index >= rows_number(header))
        return WRITE_ROW_OUT_OF_BOUND;
    uint8_t* page = (uint8_t*) get_pages(manager, header->offset, header->scale);
    if (!is_row_empty(page, row->index)) {
        return WRITE_ROW_NOT_EMPTY;
    }
    if(!set_into_bitmap(page, row->index, 1))
        return WRITE_BITMAP_ERROR;
    memcpy(page+row_offset(header, row->index), row->data, header->row_size);
    if (find_empty_row(manager, header) == -1)                                          // TODO: may be optimised
        return WRITE_ROW_OK_BUT_FULL;
    return WRITE_ROW_OK;
}

RowRemoveStatus remove_row(MemoryManager* manager, const PageMeta* header, uint32_t row_ind) {
    if (row_ind >= rows_number(header))
        return REMOVE_ROW_OUT_OF_BOUND;
    int64_t empty_row = find_empty_row(manager, header);
    uint8_t* page = (uint8_t*) get_pages(manager, header->offset, header->scale);
    memset(page + row_offset(header, row_ind), 0, header->row_size);
    if(!set_into_bitmap(page, row_ind, 0))
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
    for (uint8_t* bitmap = page + sizeof(PageRecord); bitmap < page + sizeof(PageRecord) + (rows / 8); bitmap++) {
        uint8_t mask = 0b10000000;
        uint8_t in_byte = 0;
        do {
            if (((*bitmap) & mask) == 0)
                return i+in_byte;
            in_byte++;
        } while ((mask >>= 1) != 0);
        i += 8;
    }
    return -1;
}

RowWriteResult find_and_write_row(MemoryManager* manager, const PageMeta* header, void* data) {
    RowWriteResult result = {0};
    int64_t row_id = find_empty_row(manager, header);
    if (row_id == -1){
        result.status = WRITE_ROW_OUT_OF_BOUND;
        return result;
    }
    result.row_id = row_id;
    PageRow row = {.data = data, .index = row_id};
    result.status = write_row(manager, header, &row);
    return result;
}

uint32_t next_page_index(MemoryManager* manager, uint32_t current_page) {
    return map_page_header(manager, current_page)->next;
}