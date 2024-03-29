//
// Created by vlad on 23.11.23.
//

#include <fcntl.h>
#include <malloc.h>
#include <sys/stat.h>
#include <memory.h>
#include "storage_intlzr.h"
#include "util.h"
#include "search_filters.h"

void create_scheme_table(Storage *storage);

// must be called at first!
static RowWriteStatus create_tables_table(Storage *storage) {
    PageMeta pg = storage_add_page(storage, 1, sizeof(TableRecord));
    if (pg.offset != RESERVED_TO_FILE_META) {
        panic("TABLE FOR TABLES CREATED AT NON-FIRST DATA PAGE", 6);
    }
    if (get_header(storage)->tables_number > 0) {
        panic("ATTEMPT TO CREAT TABLE FOR TABLES WHEN SOME TABLE ALREADY EXISTS", 6);
    }
    TableRecord table = {
            .first_full_pg=0,
            .first_free_pg=pg.offset,
            .table_id=0,
            .fields_n=7,
            .row_size=sizeof(TableRecord),
            .page_scale=1,
            .name="tables"
    };
    PageRow row = {.index=0, .data=(void*)&table};
    RowWriteStatus result = write_row(&storage->manager, &pg, &row);
    if (result != WRITE_ROW_OK) {
        panic("CANT CREATE TABLE OF TABLES", 6);
    }
    get_header(storage)->tables_number++;
    return result;
}

// returns 1 if table exists
// 0 if nothing to load
static void open_tables_table(Storage* storage) {
    storage->tables.scheme = get_table_of_tables_scheme().fields;
    TableRecord stub = {
            .first_free_pg = RESERVED_TO_FILE_META,
            .row_size = sizeof(TableRecord),
            .fields_n = 7,
            .page_scale = 1,
            .table_id = 0
    };
    storage->tables.mapped_addr = &stub;
    RowsIterator* iter = create_rows_iterator(storage, &storage->tables);
    char* tablename = "tables";
    rows_iterator_add_filter(iter, equals_filter, tablename, "name");
    if (!map_table(storage, iter, &storage->tables)) {
        panic("CANT OPEN TABLE FOR TABLE, NOT FOUND!",  6);
    }
    rows_iterator_free(iter);
}


static void create_new_storage(Storage* storage, char* filename) {
    int fd = open(filename, O_RDWR | O_CREAT, 0777);
    if (fd == -1) {
        panic("cant create database file!", 6);
    }
    storage->manager = init_memory_manager(fd);
    storage->header_chunk = load_chunk(&storage->manager, 0, RESERVED_TO_FILE_META);
    FileHeader* header = get_header(storage);
    header->magic_number = FILE_HEADER_MAGIC_NUMBER;
    header->pages_number = RESERVED_TO_FILE_META;
    header->tables_number = 0;
    header->data_offset = RESERVED_TO_FILE_META;

    create_tables_table(storage);
    open_tables_table(storage);
    create_scheme_table(storage);

    TableScheme scheme = create_table_scheme(2);
    add_scheme_field(&scheme, "page_ind", TABLE_FTYPE_UINT_32, 0);
    add_scheme_field(&scheme, "page_scale", TABLE_FTYPE_UINT_16, 0);
    Table* table = init_table(&scheme, "free_pages");
    create_table(storage, table, &storage->free_page_table);
    destruct_table(table);
}

void create_scheme_table(Storage *storage) {
    TableScheme scheme = get_scheme_table_scheme();
    Table* st = init_table(&scheme, "scheme_table");
    write_table(storage, st, &storage->scheme_table);
    storage->scheme_table.scheme = scheme.fields;
    free(st->name);
    free(st);
}

static void open_scheme_table(Storage *storage) {
    RowsIterator* iter = create_rows_iterator(storage, &storage->tables);
    char* scheme_table_name = "scheme_table";
    rows_iterator_add_filter(iter, equals_filter, scheme_table_name, "name");
    if (!map_table(storage, iter, &storage->scheme_table)) {
        panic("CANT LOAD SCHEME TABLE", 6);
    }
    rows_iterator_free(iter);
    storage->scheme_table.scheme = get_scheme_table_scheme().fields;
}

Storage* init_storage(char* filename) {
    Storage* storage = malloc(sizeof(Storage));
    storage->free_page_table.mapped_addr = NULL;
    storage->free_page_table.scheme = NULL;
    struct stat stat_buf;
    // if file not exists
    if (stat(filename, &stat_buf) < 0) {
        create_new_storage(storage, filename);
        return storage;
    }
    int fd = open(filename, O_RDWR);
    storage->manager = init_memory_manager(fd);
    storage->header_chunk = load_chunk(&storage->manager, 0, RESERVED_TO_FILE_META);
    if (get_header(storage)->magic_number != FILE_HEADER_MAGIC_NUMBER) {
        panic("WRONG FILE PASSED INTO THE STORAGE", 6);
    }
    open_tables_table(storage);
    open_scheme_table(storage);
    if (storage->free_page_table.mapped_addr == NULL) {
        open_table(storage, "free_pages", &storage->free_page_table);
    }
    return storage;
}

void close_storage(Storage* storage) {
    close_table(&storage->scheme_table);
    close_table(&storage->free_page_table);
    close_table(&storage->tables);
    remove_chunk(&storage->header_chunk);
    close(storage->manager.file_descriptor);
    destroy_memory_manager(&storage->manager);
    free(storage);
}
