//
// Created by vlad on 23.11.23.
//

#include <fcntl.h>
#include <string.h>
#include "storage_intlzr.h"
#include "util.h"
#include "search_filters.h"

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
void load_tables_table(Storage* storage) {
    TableScheme scheme = create_table_scheme(7);
    add_scheme_field(&scheme, "name", TABLE_FTYPE_CHARS, 0);
    set_last_field_size(&scheme, 32);
    add_scheme_field(&scheme, "table_id", TABLE_FTYPE_UINT_16, 0);
    add_scheme_field(&scheme, "fields_n", TABLE_FTYPE_UINT_16, 0);
    add_scheme_field(&scheme, "page_scale", TABLE_FTYPE_UINT_16, 0);
    add_scheme_field(&scheme, "row_size", TABLE_FTYPE_UINT_32, 0);
    add_scheme_field(&scheme, "first_free_pg", TABLE_FTYPE_UINT_32, 0);
    add_scheme_field(&scheme, "first_full_pg", TABLE_FTYPE_UINT_32, 0);
    storage->tables.scheme = scheme.fields;

    TableRecord stub = {0};
    stub.first_free_pg = RESERVED_TO_FILE_META;
    storage->tables.mapped_addr = &stub;
    RequestIterator* iter = create_request_iterator(storage, &storage->tables);
    char* tablename = "tables";
    char* fieldname = "name";
    request_iterator_add_filter(iter, equals_filter, tablename, fieldname);
    if (!map_table(storage, iter, &storage->tables)) {
        panic("CANT OPEN TABLE FOR TABLE, NOT FOUND!",  6);
    }
    request_iterator_free(iter);
}

//void create_heaps_table(Storage* storage) {
//    char* name = "rowdata_tables_table";
//    uint16_t fields_n = 4;
//    uint16_t pg_scale = 1;
//    uint16_t row_size = 1;
//    uint16_t first_pg = 0;
//    void* data[] = {&name, &get_header(storage)->tables_number, &fields_n,
//                    &pg_scale,&row_size, &first_pg, &first_pg};
//    table_insert_row(storage, &storage->tables, data);
//    get_header(storage)->tables_number++;
//}
//
//void load_heaps_table(Storage* storage) {
//    if (!map_table(storage, "rowdata_tables_table", &storage->heaps_table)) {
//        panic("CANT OPEN TABLE FOR TABLE, NOT FOUND!",  6);
//    }
//    TableScheme scheme = create_table_scheme(4);
//    add_scheme_field(&scheme, "page_scale", TABLE_FTYPE_UINT_16, 0);
//    add_scheme_field(&scheme, "row_size", TABLE_FTYPE_UINT_32, 0);
//    add_scheme_field(&scheme, "first_free_pg", TABLE_FTYPE_UINT_32, 0);
//    add_scheme_field(&scheme, "first_full_pg", TABLE_FTYPE_UINT_32, 0);
//    storage->heaps_table.scheme = scheme.fields;
//}

void create_new_storage(Storage* storage, char* filename) {
    int fd = open(filename, O_RDWR | O_CREAT);
    init_memory_manager(fd);
    storage->header_chunk = load_chunk(&storage->manager, 0, RESERVED_TO_FILE_META, 1);
    FileHeader* header = get_header(storage);
    header->magic_number = FILE_HEADER_MAGIC_NUMBER;
    header->pages_number = RESERVED_TO_FILE_META;
    header->tables_number = 0;
    header->data_offset = RESERVED_TO_FILE_META;

    create_tables_table(storage);
    load_tables_table(storage);

    TableScheme st_scheme = create_table_scheme(5);
    add_scheme_field(&st_scheme, "name", TABLE_FTYPE_STRING, 0);
    add_scheme_field(&st_scheme, "type", TABLE_FTYPE_BYTE, 0);
    add_scheme_field(&st_scheme, "table_id", TABLE_FTYPE_UINT_32, 0);
    add_scheme_field(&st_scheme, "nullable", TABLE_FTYPE_BOOL, 0);
    add_scheme_field(&st_scheme, "order", TABLE_FTYPE_UINT_16, 0);
    Table* st = init_table(&st_scheme, "scheme_table");
    // FIXME
}

Storage* init_storage(char* filename) {
//    Storage* storage = malloc(sizeof(Storage));
//    struct stat stat_buf;
//    // if file not exists
//    if (stat(filename, &stat_buf) < 0) {
//        create_new_storage(storage, filename);
//    } else {
//        int fd = open(filename, O_RDWR);
//        storage->manager = init_memory_manager(fd);
//    }
    // **ALREADY DID IT IN CREATE METHOD!!! FIX IT!**
//    storage->header_chunk = load_chunk(&storage->manager, 0, RESERVED_TO_FILE_META, 1);
//    void* header = storage->header_chunk->pointer;
//    if (*(uint16_t*)header != FILE_HEADER_MAGIC_NUMBER) {
//        panic("WRONG FILE PASSED INTO THE STORAGE", 4);
//    }
//    return storage;
}