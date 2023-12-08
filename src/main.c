#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "storage_intlzr.h"
#include "storage.h"
#include "search_filters.h"
#include "util.h"
#include "join.h"

void test_remove_and_read(Storage* storage, OpenedTable* table);

int main() {
    Storage* storage = init_storage("D:\\db1");
    OpenedTable table1;
    float f = 200.042f;
    char* s = "table1 TWO record bitchhh";
    char* chars = "plain array";
    int32_t i;
    uint16_t ui = 123;
    uint32_t counter = 0;

    if (!open_table(storage, "test_table4", &table1)) {
        TableScheme scheme = create_table_scheme(5);
        add_scheme_field(&scheme, "one", TABLE_FTYPE_FLOAT, 1);
        add_scheme_field(&scheme, "two", TABLE_FTYPE_INT_32, 1);
        add_scheme_field(&scheme, "three", TABLE_FTYPE_STRING, 1);
        add_scheme_field(&scheme, "four", TABLE_FTYPE_UINT_16, 1);
        add_scheme_field(&scheme, "five", TABLE_FTYPE_CHARS, 1);
        set_last_field_size(&scheme, 12);
        Table* ttable = init_table(&scheme, "test_table4");
        create_table(storage, ttable, &table1);
        destruct_table(ttable);
    }
    for (i = 0; i < 1000; i++) {
        f -= 0.653f;
        void *row[] = {&f, &i, s, &ui, chars};
        table_insert_row(storage, &table1, row);
        counter++;
    }
    test_remove_and_read(storage, &table1);
//    printf("\n%u rows wrote\n", counter);
//    open_table(storage, "test_table4", &table2);
//    Join* join = join_tables(storage, &table1, &table2, "two");
//    while (join_next_row(join) == REQUEST_ROW_FOUND && counter < 1000) {
//        void** row = join->row;
//        counter++;
//        printf("[%u]f=%f; i=%d; s=%s; f=%f; i=%d; s=%s\n", counter, *(float*)row[0], *(int32_t*)row[1],
//               (char*)row[2], *(float*)row[3], *(int32_t*)row[4], (char*)row[5]);
//    }
//    join_free(join);
//    test_remove_and_read(storage, &table1);

    close_table(storage, &table1);
//    close_table(storage, &table2);
    close_storage(storage);
    return 0;
}

void test_remove_and_read(Storage* storage, OpenedTable* table) {
    uint32_t counter = 0;
    RequestIterator* iterator = create_request_iterator(storage, table);
    int32_t lower_bound = 0;
    float f;
    char* s;
    int32_t i;
//    request_iterator_add_filter(iterator, greater_filter, &lower_bound, "two");
    while (request_iterator_next(iterator) == REQUEST_ROW_FOUND) {
        f = *(float*)iterator->found[0];
        i = *(int32_t*)iterator->found[1];
        s = (char*)iterator->found[2];
//        printf("row: %f %d '%s'\n", f , i, s);
        request_iterator_remove_current(iterator);
        counter++;
    }
    printf("\n%u rows removed\n", counter);
    counter = 0;
    request_iterator_free(iterator);

    iterator = create_request_iterator(storage, table);
    while (request_iterator_next(iterator) == REQUEST_ROW_FOUND) {
        f = *(float*)iterator->found[0];
        i = *(int32_t*)iterator->found[1];
        s = (char*)iterator->found[2];
//        if (i > 400)
//            printf("row: %f %d '%s'\n", f , i, s);
        counter++;
    }
    printf("\n%u rows got\n", counter);
    request_iterator_free(iterator);
}