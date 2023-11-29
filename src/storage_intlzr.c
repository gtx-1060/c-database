//
// Created by vlad on 23.11.23.
//

#include <fcntl.h>
#include <malloc.h>
#include <sys/stat.h>
#include "storage_intlzr.h"
#include "util.h"
#include "search_filters.h"

void create_scheme_table(Storage *storage);

// must be called at first!
RowWriteStatus create_tables_table(Storage *storage) {
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
    if (result == WRITE_ROW_OK) {
        get_header(storage)->tables_number++;
    }
    return result;
}

// returns 1 if table exists
// 0 if nothing to load
void open_tables_table(Storage* storage) {
    storage->tables.scheme = get_table_of_tables_scheme().fields;
    TableRecord stub = {0};
    stub.first_free_pg = RESERVED_TO_FILE_META;
    storage->tables.mapped_addr = &stub;
    RequestIterator* iter = create_request_iterator(storage, &storage->tables);
    char* tablename = "tables";
    request_iterator_add_filter(iter, equals_filter, tablename, "name");
    if (!map_table(storage, iter, &storage->tables)) {
        panic("CANT OPEN TABLE FOR TABLE, NOT FOUND!",  6);
    }
    request_iterator_free(iter);
}


void create_new_storage(Storage* storage, char* filename) {
    int fd = open(filename, O_RDWR | O_CREAT);
    storage->manager = init_memory_manager(fd);
    storage->header_chunk = load_chunk(&storage->manager, 0, RESERVED_TO_FILE_META, 1);
    FileHeader* header = get_header(storage);
    header->magic_number = FILE_HEADER_MAGIC_NUMBER;
    header->pages_number = RESERVED_TO_FILE_META;
    header->tables_number = 0;
    header->data_offset = RESERVED_TO_FILE_META;

    create_tables_table(storage);
    open_tables_table(storage);
    create_scheme_table(storage);
}

void create_scheme_table(Storage *storage) {
    TableScheme scheme = get_scheme_table_scheme();
    Table* st = init_table(&scheme, "scheme_table");
    write_table(storage, st, &storage->scheme_table);
    storage->scheme_table.scheme = scheme.fields;
    free(st);
}

void open_scheme_table(Storage *storage) {
    RequestIterator* iter = create_request_iterator(storage, &storage->tables);
    char* scheme_table_name = "scheme_table";
    request_iterator_add_filter(iter, equals_filter, &scheme_table_name, "name");
    if (!map_table(storage, iter, &storage->scheme_table)) {
        panic("CANT LOAD SCHEME TABLE", 6);
    }
    request_iterator_free(iter);
    storage->scheme_table.scheme = get_scheme_table_scheme().fields;
}

Storage* init_storage(char* filename) {
    Storage* storage = malloc(sizeof(Storage));
    struct stat stat_buf;
    // if file not exists
    if (stat(filename, &stat_buf) < 0) {
        create_new_storage(storage, filename);
        return storage;
    }
    int fd = open(filename, O_RDWR);
    storage->manager = init_memory_manager(fd);
    storage->header_chunk = load_chunk(&storage->manager, 0, RESERVED_TO_FILE_META, 1);
    if (get_header(storage)->magic_number != FILE_HEADER_MAGIC_NUMBER) {
        panic("WRONG FILE PASSED INTO THE STORAGE", 6);
    }
    open_tables_table(storage);
    open_scheme_table(storage);
    return storage;
}

void free_storage(Storage* storage) {
    close_table(storage, &storage->scheme_table);
    close_table(storage, &storage->tables);
    remove_chunk(&storage->manager, storage->header_chunk->id);
    destroy_memory_manager(&storage->manager);
    free(storage);
}