#include <time.h>
#include <stdio.h>
#include <assert.h>
#include "../storage.h"
#include "../search_filters.h"
#include "../storage_intlzr.h"

#define ROWS 10000
#define STR_MAX 3000
#define STR_MIN 10

static void fill_table(Storage* storage, OpenedTable* table);
static void check_table_content(Storage* storage, OpenedTable* table);

int main(void) {
    Storage* storage = init_storage(getenv("DB_PATH"));
    OpenedTable table;

    if (!open_table(storage, "cons_test_table", &table)) {
        TableScheme scheme = create_table_scheme(3);
        add_scheme_field(&scheme, "strlen", TABLE_FTYPE_UINT_32, 1);
        add_scheme_field(&scheme, "hash", TABLE_FTYPE_UINT_32, 1);
        add_scheme_field(&scheme, "string", TABLE_FTYPE_STRING, 1);
        Table* table_wizard = init_table(&scheme, "cons_test_table");
        create_table(storage, table_wizard, &table);
        destruct_table(table_wizard);
    }

    fill_table(storage, &table);
    check_table_content(storage, &table);
    printf("Consistency test passed successfully!\n");

    close_table(&table);
    close_storage(storage);
    return 0;
}

static char* rand_string(char *str, size_t size)
{
    const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJK";
    if (size) {
        --size;
        for (size_t n = 0; n < size; n++) {
            int key = rand() % (int) (sizeof charset - 1);
            str[n] = charset[key];
        }
        str[size] = 0;
    }
    return str;
}

static uint32_t hash(const unsigned char* str) {
    uint32_t hash = 5381;
    int c;

    while ((c = *str++))
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash;
}

static void fill_table(Storage* storage, OpenedTable* table) {
    char* buffer = malloc(STR_MAX);
    rand_string(buffer, STR_MAX - 1);
    uint32_t offset = 4234;
    for (size_t i = 0; i < ROWS; i++) {
        char* str = buffer + (offset % (STR_MAX-STR_MIN));
        uint32_t len = strlen(str);
        uint32_t shash = hash((unsigned char*)str);
        void *row[] = {&len, &shash, str};
        table_insert_row(storage, table, row);

        offset = (offset << 3) + offset;
    }
    free(buffer);
}

static void check_table_content(Storage* storage, OpenedTable* table) {
    uint32_t counter = 0;
    RowsIterator* iterator = create_rows_iterator(storage, table);
    while (rows_iterator_next(iterator) == REQUEST_ROW_FOUND) {
        uint32_t len = *(uint32_t*)iterator->row[0];
        uint32_t shash = *(uint32_t*)iterator->row[1];
        char* str = iterator->row[2];
        uint32_t actual_len = strnlen(str, STR_MAX);
        assert(len == actual_len);
        assert(hash((unsigned char*)str) == shash);
        rows_iterator_remove_current(iterator);
        counter++;
    }
    rows_iterator_free(iterator);
    assert(counter == ROWS);
}
