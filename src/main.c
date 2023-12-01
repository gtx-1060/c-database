#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "storage_intlzr.h"
#include "storage.h"
#include "search_filters.h"
#include "util.h"

int main() {
    Storage* storage = init_storage("/home/vlad/Music/db");
    OpenedTable table;
    float f = 1.342f;
    char* s = "clockck";
    int32_t i = -421;
    uint32_t counter = 0;

    if (!open_table(storage, "test_table3", &table)) {
        TableScheme scheme = create_table_scheme(3);
        add_scheme_field(&scheme, "one", TABLE_FTYPE_FLOAT, 1);
        add_scheme_field(&scheme, "two", TABLE_FTYPE_INT_32, 1);
        add_scheme_field(&scheme, "three", TABLE_FTYPE_STRING, 1);
        Table* ttable = init_table(&scheme, "test_table3");
        create_table(storage, ttable, &table);
        destruct_table(ttable);
    }
    for (i = -1000; i < 1000; i++) {
        f -= 0.653f;
        void *row[] = {&f, &i, s};
        table_insert_row(storage, &table, row);
        counter++;
    }
    printf("\n%u rows wrote\n", counter);

    counter = 0;
    RequestIterator* iterator = create_request_iterator(storage, &table);
    int32_t lower_bound = 500;
    request_iterator_add_filter(iterator, greater_filter, &lower_bound, "two");
    while (request_iterator_next(iterator) == REQUEST_ROW_FOUND) {
        void* newrow[] = {NULL, NULL, "greater than 500"};
        f = *(float*)iterator->found[0];
        i = *(int32_t*)iterator->found[1];
        s = (char*)iterator->found[2];
        printf("row: %f %d '%s'\n", f , i, s);
        request_iterator_replace_current(iterator, newrow);
        counter++;
    }
    printf("\n%u rows replaced\n", counter);
    counter = 0;
    request_iterator_free(iterator);

    iterator = create_request_iterator(storage, &table);
    while (request_iterator_next(iterator) == REQUEST_ROW_FOUND) {
        f = *(float*)iterator->found[0];
        i = *(int32_t*)iterator->found[1];
        s = (char*)iterator->found[2];
        if (i > 400)
            printf("row: %f %d '%s'\n", f , i, s);
        counter++;
    }
    printf("\n%u rows got\n", counter);

    request_iterator_free(iterator);
    close_table(storage, &table);
    close_storage(storage);
    return 0;
}
