//
// Created by vlad on 12.10.23.
//

#include <fcntl.h>
#include <malloc.h>
#include <sys/stat.h>
#include <string.h>
#include "storage.h"
#include "util.h"

void init_memory_managers(Storage* storage, int fd) {
    storage->header_manager = init_memory_manager(
            fd,
            RESERVED_TO_TABLES + RESERVED_TO_FILE_META,
            0
    );
    storage->page_manager = init_memory_manager(fd, DEFAULT_CHUNK_SIZE, 1);
}

// return fd
void create_new_storage(char* filename, Storage* storage) {
    int fd = open(filename, O_RDWR | O_CREAT);
    init_memory_managers(storage, fd);
    uint16_t* header = (uint16_t*)get_mapped_pages(storage->header_manager, 0, 1);
    // define magic number
    *header = FILE_HEADER_MAGIC_NUMBER;
    // define data_offset
    *(header+1) = RESERVED_TO_FILE_META + RESERVED_TO_TABLES;
    // tables number
    *(header+2) = 0;
    // define actual number of pages
    *((uint32_t*)(header+2)) = RESERVED_TO_FILE_META + RESERVED_TO_TABLES;
    // set all the reserved for tables metadata memory to zero
    void* tables_reserved = get_mapped_pages(storage->header_manager,
     RESERVED_TO_FILE_META, RESERVED_TO_TABLES);
    memset(tables_reserved, 0, RESERVED_TO_TABLES*SYS_PAGE_SIZE);
}

Storage* init_storage(char* filename) {
    Storage* storage = malloc(sizeof(Storage));
    struct stat stat_buf;
    // if file not exists
    if (stat(filename, &stat_buf) < 0) {
        create_new_storage(filename, storage);
    } else {
        int fd = open(filename, O_RDWR);
        init_memory_managers(storage, fd);
    }
    uint16_t* header = (uint16_t*)get_mapped_pages(storage->header_manager, 0, 1);
    if (*header != FILE_HEADER_MAGIC_NUMBER) {
        panic("WRONG FILE PASSED INTO THE STORAGE", 4);
    }
    storage->header.data_offset = header+1;
    storage->header.pages_number = (uint32_t*)(header+2);
    return storage;
}

void destruct_storage(Storage* storage) {
    destruct_memory_manager(storage->header_manager);
    destruct_memory_manager(storage->page_manager);
    free(storage);
}

PageMeta storage_add_page(Storage* storage, uint16_t scale, uint32_t row_size, uint32_t next) {
    PageMeta page = {
            .scale = scale,
            .row_size = row_size,
            .offset = *storage->header.pages_number
    };
    (*storage->header.pages_number)++;
    create_page(storage->page_manager, &page);
    return page;
}

PageMeta table_write_scheme_page(Storage* storage, TableMeta* table) {
    uint32_t scheme_need_mem = sizeof(TableSchemeField)*table->fields_n;
    uint16_t pg_scale;
    for (pg_scale = 1; pg_scale * SYS_PAGE_SIZE < scheme_need_mem; pg_scale++) ;
    PageMeta scheme_page = storage_add_page(storage, pg_scale, sizeof(TableSchemeField), 0);
    if (scheme_page.offset == 0)
        panic("CANNOT CREATE PAGE FOR SCHEMES", 4);

    for (uint16_t i = 0; i < table->fields_n; i++) {
        PageRow row = {
                .size = scheme_page.row_size,
                .index = i,
                .data = table->fields+i
        };
        RowWriteResult res = write_row(storage->page_manager, &scheme_page, &row);
        if (res != WRITE_ROW_OK && res != WRITE_ROW_OK_BUT_FULL) {
            printf("i=%d field=%s write_err=%d", i, table->fields[i].name, res);
            panic("CANNOT WRITE SCHEME", 4);
        }
    }
    return scheme_page;
}

// NOT COMPATIBLE WITH SPACE IN TABLES ARRAY!!!!
MappedTableMeta create_table(Storage* storage, TableMeta* table) {
    int8_t* pointer = (int8_t*)get_mapped_pages(
        storage->header_manager,
        RESERVED_TO_FILE_META,
        RESERVED_TO_TABLES
    );
    pointer += ONE_TABLE_META_ON_DRIVE_SIZE * (*storage->header.tables_number);
    if (*pointer != 0)
        panic("TABLES POINTER AT THE HEADER POINTS TO THE WRONG PLACE", 4);
    (*storage->header.pages_number)++;

    strncpy((char*)pointer, table->name, 32);
    pointer += 32;
    *((uint16_t*)pointer) = table->fields_n;
    pointer += sizeof(uint16_t);
    *((uint16_t*)pointer) = table->page_scale;
    pointer += sizeof(uint16_t);
    *((uint32_t*)pointer) = table->row_size;
    pointer += sizeof(uint32_t);
    // create page only for table scheme
    *((uint32_t*)pointer) = table_write_scheme_page(storage, table).offset;
    pointer += sizeof(uint32_t);

    // create first data page!
    PageMeta page = storage_add_page(storage, table->page_scale, table->row_size, 0);
    if (page.offset == 0)
        panic("CANT CREATE FIRST PAGE OF THE TABLE", 4);
    *((uint32_t*)pointer) = page.offset;
    pointer += sizeof(uint32_t);
    *((uint32_t*)pointer) = 0;
    MappedTableMeta mtable = {
            .table_meta = table,
            .first_free_page = (uint32_t*)(pointer-sizeof(uint32_t)),
            .first_full_page = (uint32_t*)pointer
    };
    return mtable;
}

MappedTableMeta read_table(const Storage* storage, uint8_t* pointer) {
    TableMeta* table = malloc(sizeof(TableMeta));
    strncpy(table->name, (char*)pointer, 32);
    pointer += 32;
    table->fields_n = *((uint16_t*)pointer);
    pointer += sizeof(uint16_t);
    table->page_scale = *((uint16_t*)pointer);
    pointer += sizeof(uint16_t);
    table->row_size = *((uint32_t*)pointer);
    pointer += sizeof(uint32_t);
    table->scheme_page = *((uint32_t*)pointer);
    pointer += sizeof(uint32_t);
    MappedTableMeta mtable = {
            .table_meta = table,
            .first_free_page = (uint32_t*)pointer,
            .first_full_page = (uint32_t*)(pointer+sizeof(uint32_t))
    };
    return mtable;
}

// NOT COMPATIBLE WITH SPACE IN TABLES ARRAY!!!!
MappedTableMeta find_table(const Storage* storage, char* name) {
    uint8_t* pointer = (uint8_t*)get_mapped_pages(
            storage->header_manager,
            RESERVED_TO_FILE_META,
            RESERVED_TO_TABLES
    );
    while (*pointer != 0) {
        if (strncmp(name, (char*)pointer, 32) == 0)
            return read_table(storage, pointer);
        pointer += ONE_TABLE_META_ON_DRIVE_SIZE;
    }
}


