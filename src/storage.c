//
// Created by vlad on 12.10.23.
//

#include <malloc.h>
#include <string.h>
#include "storage.h"
#include "request_iterator.h"
#include "util.h"
#include "search_filters.h"

void write_table(Storage* storage, Table* table, OpenedTable* dest);

FileHeader* get_header(Storage* storage) {
    return (FileHeader*)storage->header_chunk->pointer;
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

void move_page_to_full_list(Storage* storage, PageMeta* page, const OpenedTable* table) {
    // check whatever it's full
    if (find_empty_row(&storage->manager, page) != -1) {
        panic("ATTEMPT TO MOVE NOT FULL PAGE INTO FULL_LIST", 4);
    }
    remove_from_table_list(storage, page, &table->mapped_addr->first_free_pg);
    push_to_table_list(storage, page, &table->mapped_addr->first_full_pg);
}

void move_page_to_free_list(Storage* storage, PageMeta* page, const OpenedTable* table) {
    // check whatever it's full
    if (find_empty_row(&storage->manager, page) == -1) {
        panic("ATTEMPT TO MOVE FULL PAGE INTO FREE_LIST", 4);
    }
    remove_from_table_list(storage, page, &table->mapped_addr->first_full_pg);
    push_to_table_list(storage, page, &table->mapped_addr->first_free_pg);
}

PageMeta create_table_page(Storage* storage, const OpenedTable* table) {
    PageMeta pg = storage_add_page(storage, table->mapped_addr->page_scale, table->mapped_addr->row_size);
    push_to_table_list(storage, &pg, &table->mapped_addr->first_free_pg);
    return pg;
}

void get_heap_table(Storage* storage, OpenedTable* heap_table, size_t str_len) {
    // we need heap_table with at least
    // row_size > size(str_len) + size(table_id)
    size_t lower_bound = str_len + sizeof(uint32_t);
    size_t upper_bound = lower_bound+HEAP_ROW_SIZE_STEP;
    char* table_name = HEAP_TABLES_NAME;
    RequestIterator* iter = create_request_iterator(storage, &storage->tables);
    request_iterator_add_filter(iter, equals_filter, table_name, "name");
    request_iterator_add_filter(iter, greater_filter, &str_len, "row_size");
    request_iterator_add_filter(iter, less_filter, &upper_bound, "row_size");
    if (!map_table(storage, iter, heap_table)) {
        TableScheme scheme = get_heap_table_scheme(str_len);
        Table* table = init_table(&scheme, HEAP_TABLES_NAME);
        write_table(storage, table, heap_table);
        heap_table->scheme = table->fields;
        // do free instead of destruct_table(...) call
        // because we won't free scheme memory
        free(table);
        request_iterator_free(iter);
        return;
    }
    heap_table->scheme = get_heap_table_scheme(str_len).fields;
    request_iterator_free(iter);
}

// take refs from array and write consistently into memory
// do formatting and other additional if necessary
// ALSO INSERT STRINGS INTO HEAP_TABLE!
void* prepare_row_for_insertion(Storage* storage, const OpenedTable* table, void* array[]) {
    void* memory = malloc(table->mapped_addr->row_size);
    char* pointer = (char*)memory;
    order_scheme_items(table->scheme, table->mapped_addr->fields_n);
    for (uint32_t i = 0; i < table->mapped_addr->fields_n; i++) {
        if (table->scheme[i].order != i) {
            panic("wrong scheme items order", 4);
        }
        if (table->scheme[i].type == TABLE_FTYPE_STRING) {
            // create or load heap_table for string values
            OpenedTable heap_table = {0};
            uint16_t str_len = strlen(array[i]);
            get_heap_table(storage, &heap_table, str_len);
            void* data[] = {&str_len, array[i], &table->mapped_addr->table_id};
            // insert string into table and close it
            InsertRowResult result = table_insert_row(storage, &heap_table, data);
            close_table(storage, &heap_table);
            // copy (page_id,row_id) into memory to insert in target table
            memcpy(pointer, &result.page_id, sizeof(uint32_t));
            pointer += sizeof(uint32_t);
            memcpy(pointer, &result.row_id, sizeof(uint32_t));
            pointer += sizeof(uint32_t);
            continue;
        }
        uint32_t field_sz = table->scheme[i].actual_size;
        memcpy(pointer, array[i], field_sz);
        pointer += field_sz;
    }
    return memory;
}

InsertRowResult table_insert_row(Storage* storage, const OpenedTable* table, void* array[]) {
    void* memory = prepare_row_for_insertion(storage, table, array);
    if (table->mapped_addr->first_free_pg == 0) {
        create_table_page(storage, table);
        if (table->mapped_addr->first_free_pg == 0)
            panic("first free table page is still null", 4);
    }

    PageMeta page_header;
    read_page_meta(&storage->manager, table->mapped_addr->first_free_pg, &page_header);
    RowWriteResult wresult = find_and_write_row(&storage->manager, &page_header, memory);
    free(memory);
    switch (wresult.status) {
        case WRITE_ROW_NOT_EMPTY:
        case WRITE_BITMAP_ERROR:
        case WRITE_ROW_OUT_OF_BOUND:
            panic("WRONG INSERT RECORD RESULT", 4);
            break;
        case WRITE_ROW_OK_BUT_FULL:
            move_page_to_full_list(storage, &page_header, table);
            printf("[log] Record successfully added at:\n  table %s\n  page %u (already full)\n",
                   table->mapped_addr->name, page_header.offset);
            break;
        case WRITE_ROW_OK:
            printf("[log] Record successfully added at:\n  table %s\n  page %u\n",
                   table->mapped_addr->name, page_header.offset);
    }
    InsertRowResult result = {.row_id = result.row_id, .page_id=page_header.offset};
    return result;
}

GetRowResult table_get_row(Storage* storage, const OpenedTable* table, uint32_t page_ind, uint32_t row_ind) {
    void** data = malloc(sizeof(void*)*table->mapped_addr->fields_n);
    GetRowResult result = {0};
    PageRow row = {.index=row_ind};
    PageMeta page;
    read_page_meta(&storage->manager, page_ind, &page);
    result.result = read_row(&storage->manager, &page, &row);
    if (result.result != READ_ROW_OK)
        return result;
    uint32_t pointer = 0;
    // todo move next line in scheme load
    order_scheme_items(table->scheme, table->mapped_addr->fields_n);
    for (uint32_t i = 0; i < table->mapped_addr->fields_n; i++) {
        if (table->scheme[i].order != i) {
            panic("wrong scheme items order", 4);
        }
        // handling variable length strings
        if (table->scheme[i].type == TABLE_FTYPE_STRING) {
            uint32_t str_page_ind = *(uint32_t*)((char*)row.data + pointer);
            PageRow string_row = {
                    .index = *(uint32_t*)((char*)row.data + pointer + sizeof(uint32_t))
            };
            read_page_meta(&storage->manager, str_page_ind, &page);
            if (read_row(&storage->manager, &page, &string_row) != READ_ROW_OK) {
                printf("[log] Cant read string from heap table\n");
                continue;
            }
            uint16_t str_len = *(uint16_t*)string_row.data;
            data[i] = malloc(str_len);
            memcpy(data[i], (char*)string_row.data + sizeof(uint16_t), str_len);
            pointer += table->scheme[i].actual_size;
            free_row(&string_row);
            continue;
        }
        data[i] = malloc(table->scheme[i].actual_size);
        memcpy(data[i], (char*)row.data + pointer, table->scheme[i].actual_size);
        pointer += table->scheme[i].actual_size;
    }
    free_row(&row);
    result.data = data;
    return result;
}

void remove_str_from_heap(Storage *storage, uint32_t row_ind, PageMeta *pg_header, uint32_t pointer) {
    // get row to take heap page index and row with string
    PageRow row = {.index = row_ind};
    read_row(&storage->manager, pg_header, &row);
    // get page and row of string
    uint32_t str_page_ind = *(uint32_t*)((char*)row.data + pointer);
    uint32_t str_row_ind = *(uint32_t*)((char*)row.data + pointer + sizeof(uint32_t));
    free_row(&row);
    // get row with string and heap table id
    row.index = str_row_ind;
    PageMeta str_page;
    read_page_meta(&storage->manager, str_page_ind, &str_page);
    if (read_row(&storage->manager, &str_page, &row) != READ_ROW_OK) {
        panic("CANT DELETE STRING", 4);
    }
    // str len to calculate offset from start row to table_id
    uint16_t str_len = *(uint16_t*)row.data;
    uint16_t table_id = *(uint16_t*)((char*)row.data + str_len + sizeof(uint16_t));
    free_row(&row);
    // look for the heap table and remove row with string
    RequestIterator* iter = create_request_iterator(storage, storage->tables);
    request_iterator_add_filter(iter, equals_filter, &table_id, "table_id");
    OpenedTable heap_table;
    map_table(storage, iter, &heap_table);
    heap_table.scheme = get_heap_table_scheme(str_len).fields;
    table_remove_row(storage, &heap_table, str_page_ind, str_row_ind);
    close_table(storage, &heap_table);
}

void table_remove_row(Storage* storage, const OpenedTable* table, uint32_t page_ind, uint32_t row_ind) {
    PageMeta pg_header;
    read_page_meta(&storage->manager, page_ind, &pg_header);
    order_scheme_items(table->scheme, table->mapped_addr->fields_n);
    uint32_t pointer = 0;
    for (uint32_t i = 0; i < table->mapped_addr->fields_n; i++) {
        if (table->scheme[i].type == TABLE_FTYPE_STRING) {
            remove_str_from_heap(storage, row_ind, &pg_header, pointer);
        }
        pointer += table->scheme[i].actual_size;
    }
    RowRemoveStatus result = remove_row(&storage->manager, &pg_header, row_ind);
    switch (result) {
        case REMOVE_ROW_OUT_OF_BOUND:
        case REMOVE_BITMAP_ERROR:
        case REMOVE_ROW_EMPTY:
            panic("WRONG REMOVE RECORD RESULT", 4);
            break;
        case REMOVE_ROW_OK_PAGE_FREED:
            move_page_to_free_list(storage, &pg_header, table);
            printf("[log] Record successfully removed at:\n  table %s\n  page %u\n  moved to free_list\n",
                   table->mapped_addr->name, pg_header.offset);
            break;
        case REMOVE_ROW_OK:
            printf("[log] Record successfully removed at:\n  table %s\n  page %u\n",
                   table->mapped_addr->name, pg_header.offset);
            break;
    }
}

uint8_t map_table(Storage* storage, RequestIterator* iter, OpenedTable* dest) {
    RequestIteratorResult result = request_iterator_next(iter);
    if (result == REQUEST_SEARCH_END) {
        return 0;
    }
    dest->chunk = load_chunk(&storage->manager, iter->page_pointer, 1, 1);
    dest->mapped_addr =
            (TableRecord*)get_row_of_mapped_page(&storage->manager, dest->chunk, iter->row_pointer-1);
    return 1;
}

// call it after creating mapping for the table
// using map_table(...) function
uint8_t table_load_scheme(Storage* storage, OpenedTable* dest) {
    TableScheme scheme = create_table_scheme(dest->mapped_addr->fields_n);
    RequestIterator* iter = create_request_iterator(storage, &storage->scheme_table);
    request_iterator_add_filter(iter, equals_filter, &dest->mapped_addr->table_id, "table_id");
    for (uint32_t i = 0; i < dest->mapped_addr->fields_n; i++) {
        if (request_iterator_next(iter) == REQUEST_SEARCH_END) {
            request_iterator_free(iter);
            return 0;
        }
        void** row = iter->found;
        insert_scheme_field(&scheme, (char*)row[0], *(uint8_t*)row[1],
                            *(uint8_t*)row[2], *(uint16_t*)row[3]);
    }
    request_iterator_free(iter);
    return 1;
}

uint8_t open_table(Storage* storage, char* name, OpenedTable* dest) {
    RequestIterator* iter = create_request_iterator(storage, &storage->tables);
    request_iterator_add_filter(iter, equals_filter, name, "name");
    if (!map_table(storage, iter, dest)) {
        request_iterator_free(iter);
        return 0;
    }
    request_iterator_free(iter);
    if (strncmp(dest->mapped_addr->name, name, 32) != 0)
        panic("OPENED WRONG TABLE OR ANOTHER MEMORY MAPPED!", 4);
    if (!table_load_scheme(storage, dest))
        return 0;
    return 1;
}

// creates table entity without scheme items!!!
// and maps it into memory
void write_table(Storage* storage, Table * table, OpenedTable* dest) {
    uint32_t zero = 0;
    void* data[] = {
            table->name,
            &get_header(storage)->tables_number,
            &table->fields_n,
            &table->page_scale,
            &table->row_size,
            &zero,
            &zero
    };
    InsertRowResult result = table_insert_row(storage, &storage->tables, data);
    get_header(storage)->tables_number++;
    dest->chunk = load_chunk(&storage->manager, result.page_id, 1, 1);
    dest->mapped_addr =
            (TableRecord*)get_row_of_mapped_page(&storage->manager, dest->chunk, result.row_id);
}

uint8_t write_table_scheme(Storage* storage, Table* table, uint16_t table_id) {
    for (uint32_t i = 0; i < table->fields_n; i++) {
        SchemeItem* field = table->fields+i;
        void* row[] = {field->name, &field->type, &field->nullable, &i, &table_id};
        table_insert_row(storage, &storage->scheme_table, row);
    }
}

// create table and open it
void create_table(Storage* storage, Table* table, OpenedTable* dest) {
    write_table(storage, table, dest);
    write_table_scheme(storage, table, dest->mapped_addr->table_id);
    table_load_scheme(storage, dest);
}

void close_table(Storage* storage, OpenedTable* table) {
    free_scheme(table->scheme, table->mapped_addr->fields_n);
    remove_chunk(&storage->manager, table->chunk->id);
}

void free_row_array(const OpenedTable* table, void** row) {
    for (uint32_t i = 0; i < table->mapped_addr->fields_n; i++) {
        free(row[i]);
    }
    free(row);
}



