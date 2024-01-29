//
// Created by vlad on 12.10.23.
//

#include <malloc.h>
#include <string.h>
#include "storage.h"
#include "rows_iterator.h"
#include "util.h"
#include "search_filters.h"

void write_table(Storage* storage, Table* table, OpenedTable* dest);

FileHeader* get_header(Storage* storage) {
    return (FileHeader*)chunk_get_pointer(&storage->header_chunk);
}

PageMeta storage_add_page(Storage* storage, uint16_t scale, uint32_t row_size) {
    uint32_t pi;
    if (storage->free_page_table.mapped_addr && storage->free_page_table.scheme) {
        RowsIterator* iter = create_rows_iterator(storage, &storage->free_page_table);
        rows_iterator_add_filter(iter, equals_filter, &scale, "page_scale");
        if (rows_iterator_next(iter) == REQUEST_ROW_FOUND) {
            pi = *(uint32_t*)iter->row[0];
            rows_iterator_remove_current(iter);
        } else {
            pi = get_header(storage)->pages_number;
            get_header(storage)->pages_number += scale;
        }
        rows_iterator_free(iter);
    } else {
        pi = get_header(storage)->pages_number;
        get_header(storage)->pages_number += scale;
    }
    PageMeta page = {
            .scale = scale,
            .row_size = row_size,
            .offset = pi
    };
    uint32_t next = create_page(&storage->manager, &page);
    if (next == 0) {
        panic("CANT CREATE NEW PAGE!", 4);
    }
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
    uint32_t ffrp = table->mapped_addr->first_free_pg;
    uint32_t ffup = table->mapped_addr->first_full_pg;
    remove_from_table_list(storage, page, &ffrp);
    table->mapped_addr->first_free_pg = ffrp;
    push_to_table_list(storage, page, &ffup);
    table->mapped_addr->first_full_pg = ffup;
}

void move_page_to_free_list(Storage* storage, PageMeta* page, const OpenedTable* table) {
    // check whatever it's full
    if (find_empty_row(&storage->manager, page) == -1) {
        panic("ATTEMPT TO MOVE FULL PAGE INTO FREE_LIST", 4);
    }
    uint32_t ffrp = table->mapped_addr->first_free_pg;
    uint32_t ffup = table->mapped_addr->first_full_pg;
    remove_from_table_list(storage, page, &ffup);
    table->mapped_addr->first_full_pg = ffup;
    push_to_table_list(storage, page, &ffrp);
    table->mapped_addr->first_free_pg = ffrp;
}

PageMeta create_table_page(Storage* storage, const OpenedTable* table) {
    PageMeta pg = storage_add_page(storage, table->mapped_addr->page_scale, table->mapped_addr->row_size);
    uint32_t ffrp = table->mapped_addr->first_free_pg;
    push_to_table_list(storage, &pg, &ffrp);
    table->mapped_addr->first_free_pg = ffrp;
    return pg;
}

void get_heap_table(Storage* storage, OpenedTable* heap_table, size_t str_len) {
    // we need heap_table with at least
    // row_size >  + size(str_len) + size(str) + size(table_id)
    size_t lower_bound = sizeof(uint16_t) + str_len + sizeof(uint16_t);
    size_t upper_bound = lower_bound+HEAP_ROW_SIZE_STEP;
    char* table_name = HEAP_TABLES_NAME;
    RowsIterator* iter = create_rows_iterator(storage, &storage->tables);
    rows_iterator_add_filter(iter, equals_filter, table_name, "name");
    rows_iterator_add_filter(iter, greater_eq_filter, &lower_bound, "row_size");
    rows_iterator_add_filter(iter, less_filter, &upper_bound, "row_size");
    // open heap table or if aren't create
    if (!map_table(storage, iter, heap_table)) {
        printf("heap table created\n");
        TableScheme scheme = get_heap_table_scheme(str_len);
        Table* table = init_table(&scheme, HEAP_TABLES_NAME);
        write_table(storage, table, heap_table);
        heap_table->scheme = scheme.fields;
        // do free instead of destruct_table(...) call
        // because we won't free scheme memory
        free(table->name);
        free(table);
        rows_iterator_free(iter);
        return;
    }
    heap_table->scheme = get_heap_table_scheme(str_len).fields;
//    printf("heap table str size: %d, row size: %d", heap_table->scheme[1].max_sz, heap_table->mapped_addr->row_size);
    rows_iterator_free(iter);
}

// take refs from array and write consistently into memory
// do formatting and additional if necessary
// ALSO INSERT STRINGS INTO HEAP_TABLE!
void* prepare_row_for_insertion(Storage* storage, const OpenedTable* table, void* array[]) {
    void* memory = malloc(table->mapped_addr->row_size);
//    printf("row-size=%d\n", table->mapped_addr->row_size);
    char* pointer = (char*)memory;
    for (uint32_t i = 0; i < table->mapped_addr->fields_n; i++) {
        if (table->scheme[i].order != i) {
            panic("wrong scheme items order", 4);
        }
        if (table->scheme[i].type == TABLE_FTYPE_STRING) {
            // create or load heap_table for string values
            OpenedTable heap_table = {0};
            uint16_t str_len = strlen((char*)array[i])+1;
            get_heap_table(storage, &heap_table, str_len);
            void* data[] = {&str_len, array[i], &heap_table.mapped_addr->table_id};
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

        uint16_t field_sz = table->scheme[i].max_sz;
        if (table->scheme[i].type == TABLE_FTYPE_CHARS) {
            memcpy(pointer, array[i], strlen(array[i])+1);
//            printf("%d, %d\n", table->scheme[i].type, field_sz);
        } else {
//            printf("%d, %d\n", table->scheme[i].type, field_sz);
//            printf("p=%p\n", pointer);
            memcpy(pointer, array[i], field_sz);
        }
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

    PageMeta pg_header;
    read_page_meta(&storage->manager, table->mapped_addr->first_free_pg, &pg_header);
    RowWriteResult wresult = find_and_write_row(&storage->manager, &pg_header, memory);
    free(memory);
    switch (wresult.status) {
        case WRITE_ROW_NOT_EMPTY:
        case WRITE_BITMAP_ERROR:
        case WRITE_ROW_OUT_OF_BOUND:
            panic("WRONG INSERT RECORD RESULT", 4);
            break;
        case WRITE_ROW_OK_BUT_FULL:
            move_page_to_full_list(storage, &pg_header, table);
            tlog(ROW_INSERT_FULL, table->mapped_addr->name, pg_header.offset, wresult.row_id);
            break;
        case WRITE_ROW_OK:
            tlog(ROW_INSERT, table->mapped_addr->name, pg_header.offset, wresult.row_id);
    }
    InsertRowResult result = {.row_id = wresult.row_id, .page_id=pg_header.offset};
    return result;
}

RowReadStatus table_get_row_in_buff(Storage* storage, const OpenedTable* table, void* buffer[],
                                    uint32_t page_ind, uint32_t row_ind) {
    PageRow row = {.index=row_ind};
    PageMeta page;
    read_page_meta(&storage->manager, page_ind, &page);

    RowReadStatus result = read_row(&storage->manager, &page, &row);
    if (result != READ_ROW_OK)
        return result;
    char* pointer = row.data;
    for (uint32_t i = 0; i < table->mapped_addr->fields_n; i++) {
        if (table->scheme[i].order != i) {
            panic("wrong scheme items order", 4);
        }
        // handling variable length strings
        if (table->scheme[i].type == TABLE_FTYPE_STRING) {
            uint32_t str_page_ind = *(uint32_t*)pointer;
            PageRow string_row = {
                    .index = *(uint32_t*)(pointer + sizeof(uint32_t))
            };
            read_page_meta(&storage->manager, str_page_ind, &page);
            RowReadStatus s = read_row(&storage->manager, &page, &string_row);
            if (s != READ_ROW_OK) {
                printf("[log] Cant read string from heap table\n");
                continue;
            }
            uint16_t str_len = *(uint16_t*)string_row.data;
            buffer[i] = malloc(str_len);
            memcpy(buffer[i], ((char*)string_row.data) + sizeof(uint16_t), str_len);
            pointer += table->scheme[i].max_sz;
            free_row(&string_row);
            continue;
        }
        buffer[i] = malloc(table->scheme[i].max_sz);
        memcpy(buffer[i], pointer, table->scheme[i].max_sz);
        pointer += table->scheme[i].max_sz;
    }
    free_row(&row);
    return result;
}

// -1 if fails
int table_find_index_of_column(const OpenedTable* table, char* column) {
    for (SchemeItem* col = table->scheme;
         col < table->scheme + table->mapped_addr->fields_n; col++) {
        if (strcmp(col->name, column) == 0) {
            return col->order;
        }
    }
    return -1;
}

GetRowResult table_get_row(Storage* storage, const OpenedTable* table, uint32_t page_ind, uint32_t row_ind) {
    GetRowResult result = {
            .data = malloc(sizeof(void*) * table->mapped_addr->fields_n)
    };
    result.result = table_get_row_in_buff(storage, table, result.data, page_ind, row_ind);
    if (result.result != READ_ROW_OK) {
        free(result.data);
        result.data = NULL;
    }
    return result;
}

void remove_str_from_heap(Storage *storage, uint32_t row_ind, PageMeta *pg_header, uint32_t offset_in_row) {
    // get row to take heap page index and row with string
    PageRow row = {.index = row_ind};
    read_row(&storage->manager, pg_header, &row);
    // get page and row of string
    uint32_t str_page_ind = *(uint32_t*)((char*)row.data + offset_in_row);
    uint32_t str_row_ind = *(uint32_t*)((char*)row.data + offset_in_row + sizeof(uint32_t));
    free_row(&row);
    // get row with string and heap table id
    row.index = str_row_ind;
    PageMeta str_page;
    read_page_meta(&storage->manager, str_page_ind, &str_page);
    if (read_row(&storage->manager, &str_page, &row) != READ_ROW_OK) {
        panic("CANT DELETE STRING", 4);
    }
    // str len to calculate offset from start row to table_id
    char* pointer = row.data;
    uint16_t str_len = *(uint16_t*)pointer;
    pointer += sizeof(uint16_t) + get_nearest_heap_size(str_len);
    uint16_t table_id = *(uint16_t*)pointer;
    free_row(&row);
    // look for the heap table and remove row with string
    RowsIterator* iter = create_rows_iterator(storage, &storage->tables);
    rows_iterator_add_filter(iter, equals_filter, &table_id, "table_id");
    OpenedTable heap_table;
    map_table(storage, iter, &heap_table);
    heap_table.scheme = get_heap_table_scheme(str_len).fields;
    rows_iterator_free(iter);
    table_remove_row(storage, &heap_table, str_page_ind, str_row_ind);
    close_table(storage, &heap_table);
}

void handle_page_freed(Storage* storage, const OpenedTable* table, PageMeta* pg) {
    uint32_t pi = table->mapped_addr->first_free_pg;
    remove_from_table_list(storage, pg, &pi);
    table->mapped_addr->first_free_pg = pi;
    void* row[] = {&pg->offset, &pg->scale};
    table_insert_row(storage, &storage->free_page_table, row);
}

void table_remove_row(Storage* storage, const OpenedTable* table, uint32_t page_ind, uint32_t row_ind) {
    PageMeta pg_header;
    read_page_meta(&storage->manager, page_ind, &pg_header);
    uint32_t pointer = 0;
    for (uint32_t i = 0; i < table->mapped_addr->fields_n; i++) {
        if (table->scheme[i].type == TABLE_FTYPE_STRING) {
            remove_str_from_heap(storage, row_ind, &pg_header, pointer);
        }
        pointer += table->scheme[i].max_sz;
    }
    RowRemoveStatus result = remove_row(&storage->manager, &pg_header, row_ind);
//    print_bitmap(&storage->manager, page_ind);
    switch (result) {
        case REMOVE_ROW_OUT_OF_BOUND:
        case REMOVE_BITMAP_ERROR:
        case REMOVE_ROW_EMPTY:
            panic("WRONG REMOVE RECORD RESULT", 4);
            break;
        case REMOVE_ROW_OK_PAGE_NOT_FULL:
            move_page_to_free_list(storage, &pg_header, table);
            tlog(ROW_REMOVE_NOT_FULL, table->mapped_addr->name, pg_header.offset, row_ind);
            break;
        case REMOVE_ROW_OK_PAGE_FREED: {
            handle_page_freed(storage, table, &pg_header);
            tlog(ROW_REMOVE_FREED, table->mapped_addr->name, pg_header.offset, row_ind);
            break;
        }
        case REMOVE_ROW_OK:
            tlog(ROW_REMOVE, table->mapped_addr->name, pg_header.offset, row_ind);
            break;
    }
}

void table_replace_row(Storage* storage, const OpenedTable* table, uint32_t page_ind,
                       uint32_t row_ind, void* array[]) {
    PageMeta pg_header;
    read_page_meta(&storage->manager, page_ind, &pg_header);
    uint32_t pointer = 0;
    for (uint32_t i = 0; i < table->mapped_addr->fields_n; i++) {
        if (table->scheme[i].type == TABLE_FTYPE_STRING) {
            remove_str_from_heap(storage, row_ind, &pg_header, pointer);
        }
        pointer += table->scheme[i].max_sz;
    }
    PageRow row = {.index = row_ind, .data = prepare_row_for_insertion(storage, table, array)};
    switch (replace_row(&storage->manager, &pg_header, &row)) {
        case WRITE_ROW_NOT_EMPTY:
        case WRITE_BITMAP_ERROR:
        case WRITE_ROW_OUT_OF_BOUND:
            panic("WRONG REPLACE RECORD RESULT", 4);
            break;
        case WRITE_ROW_OK_BUT_FULL:
        case WRITE_ROW_OK:
            tlog(ROW_INSERT, table->mapped_addr->name, pg_header.offset, row_ind);
    }
    free(row.data);
}

uint8_t map_table(Storage* storage, RowsIterator* iter, OpenedTable* dest) {
    RowsIteratorResult result = rows_iterator_next(iter);
    if (result == REQUEST_SEARCH_END) {
        return 0;
    }

    dest->chunk = load_chunk(&storage->manager, iter->page_pointer, 1);
    dest->mapped_addr =
            (TableRecord*) get_mapped_page_row(&storage->manager, &dest->chunk, iter->row_pointer - 1);
    if (dest->mapped_addr->row_size == 0) {
        panic("MAPPED WRONG MEMORY! TABLE ROW IS NULL!", 4);
    }
    return 1;
}

// call it after creating mapping for the table
// using map_table(...) function
uint8_t table_load_scheme(Storage* storage, OpenedTable* dest) {
    TableScheme scheme = create_table_scheme(dest->mapped_addr->fields_n);
    RowsIterator* iter = create_rows_iterator(storage, &storage->scheme_table);
    rows_iterator_add_filter(iter, equals_filter, &dest->mapped_addr->table_id, "table_id");
    for (uint32_t i = 0; i < dest->mapped_addr->fields_n; i++) {
        if (rows_iterator_next(iter) == REQUEST_SEARCH_END) {
            rows_iterator_free(iter);
            free_scheme(scheme.fields, dest->mapped_addr->fields_n);
            return 0;
        }
        void** row = iter->row;
        insert_scheme_field(&scheme, (char*)row[0], *(uint8_t*)row[1],
                            *(uint8_t*)row[2], *(uint16_t*)row[3]);
    }
    rows_iterator_free(iter);
    dest->scheme = scheme.fields;
    return 1;
}

uint8_t open_table(Storage* storage, char* name, OpenedTable* dest) {
    RowsIterator* iter = create_rows_iterator(storage, &storage->tables);
    rows_iterator_add_filter(iter, equals_filter, name, "name");
    if (!map_table(storage, iter, dest)) {
        rows_iterator_free(iter);
        return 0;
    }
    rows_iterator_free(iter);
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
    dest->chunk = load_chunk(&storage->manager, result.page_id, 1);
    dest->mapped_addr =
            (TableRecord*) get_mapped_page_row(&storage->manager, &dest->chunk, result.row_id);
    if (dest->mapped_addr->row_size == 0) {
        panic("WRONG TABLE MAPPED", 4);
    }
}

void write_table_scheme(Storage* storage, Table* table, uint16_t table_id) {
    for (uint16_t i = 0; i < table->fields_n; i++) {
        SchemeItem* field = table->fields+i;
        void* row[] = {field->name, &field->type, &field->nullable, &i, &table_id};
        table_insert_row(storage, &storage->scheme_table, row);
    }
}

// create table and open it
void create_table(Storage* storage, Table* table, OpenedTable* dest) {
    RowsIterator* iter = create_rows_iterator(storage, &storage->tables);
    rows_iterator_add_filter(iter, equals_filter, &table->fields, "name");
    if (rows_iterator_next(iter) == REQUEST_ROW_FOUND) {
        rows_iterator_free(iter);
        return;
    }
    rows_iterator_free(iter);
    write_table(storage, table, dest);
    write_table_scheme(storage, table, dest->mapped_addr->table_id);
    table_load_scheme(storage, dest);
}

void close_table(Storage* storage, OpenedTable* table) {
    free_scheme(table->scheme, table->mapped_addr->fields_n);
    remove_chunk(&storage->manager, &table->chunk);
}

void free_row_content(uint16_t fields, void** row) {
    for (uint16_t i = 0; i < fields; i++) {
        if (row[i] != NULL)
            free(row[i]);
        row[i] = NULL;
    }
}



