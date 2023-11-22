//
// Created by vlad on 12.10.23.
//

#include <fcntl.h>
#include <malloc.h>
#include <sys/stat.h>
#include <string.h>
#include "storage.h"
#include "util.h"


void create_new_storage(char* filename, Storage* storage) {
    int fd = open(filename, O_RDWR | O_CREAT);
    init_memory_manager(fd);
    void* pointer = get_pages(&storage->manager, 0, 1);
    FileHeader* header = pointer;
    header->magic_number = FILE_HEADER_MAGIC_NUMBER;
    header->pages_number = RESERVED_TO_FILE_META;
    header->tables_number = 0;
    header->data_offset = RESERVED_TO_FILE_META;
    pointer = get_pages(&storage->manager, RESERVED_TO_FILE_META, 1);
    // TODO: create table of tables, scheme table, table of datatables
}

FileHeader* get_header(Storage* storage) {
    return (FileHeader*)storage->header_chunk->pointer;
}

//void set_header(Storage* storage, FileHeader* header) {
//    memcpy(storage->header_chunk->pointer, header, sizeof(FileHeader));
//}

Storage* init_storage(char* filename) {
    Storage* storage = malloc(sizeof(Storage));
    struct stat stat_buf;
    // if file not exists
    if (stat(filename, &stat_buf) < 0) {
        create_new_storage(filename, storage);
    } else {
        int fd = open(filename, O_RDWR);
        storage->manager = init_memory_manager(fd);
    }
    storage->header_chunk = load_chunk(&storage->manager, 0, RESERVED_TO_FILE_META, 1);
    void* header = storage->header_chunk->pointer;
    if (*(uint16_t*)header != FILE_HEADER_MAGIC_NUMBER) {
        panic("WRONG FILE PASSED INTO THE STORAGE", 4);
    }
    return storage;
}

void destruct_storage(Storage* storage) {
    destroy_memory_manager(&storage->manager);
}

PageMeta storage_add_page(Storage* storage, uint16_t scale, uint32_t row_size) {
    FileHeader* header = get_header(storage);
    PageMeta page = {
            .scale = scale,
            .row_size = row_size,
            .offset = header->pages_number
    };
    uint32_t next = create_page(&storage->manager, &page);
    if (next == 0) {
        panic("CANT CREATE NEW PAGE!", 4);
    }
    header->pages_number = next;
    return page;
}

void remove_from_table_list(Storage* storage, PageMeta* page, uint32_t* list_start) {
    if (page->prev == 0) {
        if (page->offset != *list_start) {
            panic("PAGE HAD .PREV=0, BUT NOT FIRST IN THE FREE_LST", 4);
        }
        *list_start = page->next;
    } else {
        map_page_header(&storage->manager, page->prev)->next = page->next;
    }
    if (page->next != 0)
        map_page_header(&storage->manager, page->next)->prev = page->prev;
}

void push_to_table_list(Storage* storage, PageMeta* page, uint32_t* list_start) {
    // if dest_list is empty
    if (*list_start == 0) {
        *list_start = page->offset;
        page->prev = 0;
        page->next = 0;
        write_page_meta(&storage->manager, page);
        return;
    }
    PageMeta list_page;
    read_page_meta(&storage->manager, *list_start, &list_page);
    if (list_page.prev != 0) {
        panic("FIRST PAGE OF FULL_LST HAVE DIFFERENT .PREV VALUE", 4);
    }
    *list_start = page->offset;
    page->prev = 0;
    page->next = list_page.offset;
    list_page.prev = page->offset;
    write_page_meta(&storage->manager, page);
    write_page_meta(&storage->manager, &list_page);
}

void move_page_to_full_list(Storage* storage, PageMeta* page, LoadedTable* table) {
    // check whatever it's full
    if (find_empty_row(&storage->manager, page) != -1) {
        panic("ATTEMPT TO MOVE NOT FULL PAGE INTO FULL_LIST", 4);
    }
    remove_from_table_list(storage, page, &table->mapped_addr->first_free_pg);
    push_to_table_list(storage, page, &table->mapped_addr->first_full_pg);
}

void move_page_to_free_list(Storage* storage, PageMeta* page, LoadedTable* table) {
    // check whatever it's full
    if (find_empty_row(&storage->manager, page) == -1) {
        panic("ATTEMPT TO MOVE FULL PAGE INTO FREE_LIST", 4);
    }
    remove_from_table_list(storage, page, &table->mapped_addr->first_full_pg);
    push_to_table_list(storage, page, &table->mapped_addr->first_free_pg);
}

PageMeta create_table_page(Storage* storage, LoadedTable* table) {
    PageMeta pg = storage_add_page(storage, table->table_meta->page_scale, table->table_meta->row_size);
    push_to_table_list(storage, &pg, &table->mapped_addr->first_free_pg);
    return pg;
}

void table_insert_row(Storage* storage, LoadedTable* table, void** data) {
    void* memory = malloc(table->table_meta->row_size);
    void* pointer = memory;
    for (uint32_t i = 0; i < table->table_meta->fields_n; i++) {
        uint32_t field_sz = table->table_meta->fields[i].actual_size;
        memcpy(pointer, data[i], field_sz);
        pointer += field_sz;
    }
    if (table->mapped_addr->first_free_pg == 0) {
        create_table_page(storage, table);
        if (table->mapped_addr->first_free_pg == 0)
            panic("first free table page is still null", 4);
    }

    PageMeta page_header;
    read_page_meta(&storage->manager, table->mapped_addr->first_free_pg, &page_header);
    RowWriteResult result = find_and_write_row(&storage->manager, &page_header, memory);
    switch (result) {
        case WRITE_ROW_NOT_EMPTY:
        case WRITE_BITMAP_ERROR:
        case WRITE_ROW_OUT_OF_BOUND:
            panic("WRONG INSERT RECORD RESULT", 4);
            break;
        case WRITE_ROW_OK_BUT_FULL:
            move_page_to_full_list(storage, &page_header, table);
            printf("[log] Record successfully added at:\n  table %s\n  page %u (already full)\n",
                   table->table_meta->name, page_header.offset);
            break;
        case WRITE_ROW_OK:
            printf("[log] Record successfully added at:\n  table %s\n  page %u\n",
                   table->table_meta->name, page_header.offset);
    }
    free(memory);
}

GotTableRow table_get_row(Storage* storage, LoadedTable* table, uint32_t page_ind, uint32_t row_ind) {
    void** data = malloc(sizeof(void*)*table->table_meta->fields_n);
    GotTableRow result = {.data=NULL};
    PageRow row = {.index=row_ind};
    PageMeta page;
    read_page_meta(&storage->manager, page_ind, &page);
    result.result = read_row(&storage->manager, &page, &row);
    if (result.result != READ_ROW_OK)
        return result;
    uint32_t pointer = 0;
    for (uint32_t i = 0; i < table->table_meta->fields_n; i++) {
        data[i] = malloc(table->table_meta->fields[i].actual_size);
        memcpy(data[i], (char*)row.data+pointer, table->table_meta->fields[i].actual_size);
        pointer += table->table_meta->fields[i].actual_size;
    }
    free_row(&row);
    result.data = data;
    return result;
}

void table_remove_row(Storage* storage, LoadedTable* table, uint32_t page_ind, uint32_t row_ind) {
    PageMeta pg_header;
    read_page_meta(&storage->manager, page_ind, &pg_header);
    RowRemoveResult result = remove_row(&storage->manager, &pg_header, row_ind);
    switch (result) {
        case REMOVE_ROW_OUT_OF_BOUND:
        case REMOVE_BITMAP_ERROR:
        case REMOVE_ROW_EMPTY:
            panic("WRONG REMOVE RECORD RESULT", 4);
            break;
        case REMOVE_ROW_OK_PAGE_FREED:
            move_page_to_free_list(storage, &pg_header, table);
            printf("[log] Record successfully removed at:\n  table %s\n  page %u\n  moved to free_list\n",
                   table->table_meta->name, pg_header.offset);
            break;
        case REMOVE_ROW_OK:
            printf("[log] Record successfully removed at:\n  table %s\n  page %u\n",
                   table->table_meta->name, pg_header.offset);
            break;
    }
}

void table_free_row_mem(LoadedTable* table, void** row) {
    for (uint32_t i = 0; i < table->table_meta->fields_n; i++) {
        free(row[i]);
    }
    free(row);
}



