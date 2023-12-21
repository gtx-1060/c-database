#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "storage_intlzr.h"
#include "storage.h"
#include "search_filters.h"

void insert_rows_example(Storage* storage, OpenedTable* table1);
void select_rows_example(Storage* storage, OpenedTable* table1);

int main() {
    Storage* storage = init_storage("/home/vlad/Music/db");
    OpenedTable table1;

    // open table or create if not exists
    if (!open_table(storage, "test_table1", &table1)) {
        TableScheme scheme = create_table_scheme(4);
        add_scheme_field(&scheme, "one", TABLE_FTYPE_FLOAT, 1);
        add_scheme_field(&scheme, "two", TABLE_FTYPE_INT_32, 1);
        add_scheme_field(&scheme, "three", TABLE_FTYPE_STRING, 1);
        add_scheme_field(&scheme, "four", TABLE_FTYPE_UINT_16, 1);
        Table* ttable = init_table(&scheme, "test_table1");
        create_table(storage, ttable, &table1);
        destruct_table(ttable);
    }

    // do something with table
    insert_rows_example(storage, &table1);
    select_rows_example(storage, &table1);

    // close table
    close_table(storage, &table1);
    close_storage(storage);
    return 0;
}

void insert_rows_example(Storage* storage, OpenedTable* table1) {
    float f = 234.234f;
    uint16_t ui = 1;
    char* s = "string! for tests!";
    for (int i = 0; i < 100; i++) {
        void *row[] = {&f, &i, s, &ui};
        table_insert_row(storage, table1, row);
        ui++;
        f += 1.2132f;
    }
}

void select_rows_example(Storage* storage, OpenedTable* table1) {
    RowsIterator* iter = create_rows_iterator(storage, table1);
    float f = 250.43f;
    rows_iterator_add_filter(iter, greater_filter, &f, "one");  // optional filter
    while (rows_iterator_next(iter) == REQUEST_ROW_FOUND) {
        printf("%f, %d, %s, %hu", *(float*)iter->found[0], *(int*)iter->found[1],
               (char*)iter->found[2], *(uint16_t *)iter->found[0]);
    }
    rows_iterator_free(iter);
}

void update_rows_example(Storage* storage, OpenedTable* table1) {
    RowsIterator* iter = create_rows_iterator(storage, table1);
    float f = 10.f;
    while (rows_iterator_next(iter) == REQUEST_ROW_FOUND) {
        void *row[] = {&f, 0, 0, 0};    // update only non-null fields
        rows_iterator_update_current(iter, row);
    }
    rows_iterator_free(iter);
}

void remove_rows_example(Storage* storage, OpenedTable* table1) {
    RowsIterator* iter = create_rows_iterator(storage, table1);
    uint16_t i = 100;
    rows_iterator_add_filter(iter, equals_filter, &i, "four");  // optional filter
    while (rows_iterator_next(iter) == REQUEST_ROW_FOUND) {
        rows_iterator_remove_current(iter);
    }
    rows_iterator_free(iter);
}