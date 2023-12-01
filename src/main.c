#include <stdio.h>
#include "storage_intlzr.h"
#include "storage.h"

int main() {
    Storage* storage = init_storage("/home/vlad/Music/db");
    TableScheme scheme = create_table_scheme(3);
    add_scheme_field(&scheme, "one", TABLE_FTYPE_FLOAT, 1);
    add_scheme_field(&scheme, "two", TABLE_FTYPE_INT_32, 1);
    add_scheme_field(&scheme, "three", TABLE_FTYPE_BYTE, 1);
    Table* table = init_table(&scheme, "test_table");
    OpenedTable open_table;
    create_table(storage, table, &open_table);
    destruct_table(table);
    float f = 100;
    char b = 'a';
    int32_t i;
    for (i = -500; i < 1000; i++) {
        f -= 0.653f;
        void* row[] = {&f, &i, &b};
        table_insert_row(storage, &open_table, row);
    }
    RequestIterator* iterator = create_request_iterator(storage, &open_table);
    while (request_iterator_next(iterator) == REQUEST_ROW_FOUND) {
        f = *(float*)iterator->found[0];
        i = *(int32_t*)iterator->found[1];
        b = *(char*)iterator->found[2];
        printf("%f, %d, %c\n", f, i, b);
    }
    request_iterator_free(iterator);
    close_table(storage, &open_table);
    close_storage(storage);
    return 0;
}
