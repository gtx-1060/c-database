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
    FileHeader header = {
            .magic_number = FILE_HEADER_MAGIC_NUMBER,
            .pages_number = RESERVED_TO_FILE_META,
            .tables_number = 0,
            .data_offset = RESERVED_TO_FILE_META
    };
    *(FileHeader*)pointer = header;
}

FileHeader get_header(Storage* storage) {
    return *(FileHeader*)storage->header_chunk->pointer;
}

void set_header(Storage* storage, FileHeader* header) {
    memcpy(storage->header_chunk->pointer, header, sizeof(FileHeader));
}

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

PageMeta storage_add_page(Storage* storage, uint16_t scale, uint32_t row_size, uint32_t next) {
    FileHeader header = get_header(storage);
    PageMeta page = {
            .scale = scale,
            .row_size = row_size,
            .offset = header.pages_number
    };
    header.pages_number++;
    create_page(&storage->manager, &page);
    set_header(storage, &header);
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
    int8_t* pointer = (int8_t*) get_pages(
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
    uint8_t* pointer = (uint8_t*) get_pages(
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


