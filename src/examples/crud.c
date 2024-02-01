#include <stdio.h>

#include "../storage_intlzr.h"
#include "../storage.h"
#include "../search_filters.h"

void insert_rows_example(Storage* storage, OpenedTable* table);
void select_rows_example(Storage* storage, OpenedTable* table, uint8_t with_filter);
void remove_rows_example(Storage* storage, OpenedTable* table);
void update_rows_example(Storage* storage, OpenedTable* table);

int main(void) {
    Storage* storage = init_storage(getenv("DB_PATH"));
    OpenedTable table;

    // open table or create if not exists
    if (!open_table(storage, "test_table", &table)) {
        TableScheme scheme = create_table_scheme(4);
        add_scheme_field(&scheme, "one", TABLE_FTYPE_FLOAT, 1);
        add_scheme_field(&scheme, "two", TABLE_FTYPE_INT_32, 1);
        add_scheme_field(&scheme, "three", TABLE_FTYPE_STRING, 1);
        add_scheme_field(&scheme, "four", TABLE_FTYPE_UINT_16, 1);
        Table* table_wizard = init_table(&scheme, "test_table");
        create_table(storage, table_wizard, &table);
        destruct_table(table_wizard);
        printf("Table created!\n");
    }

    insert_rows_example(storage, &table);
    printf("Inserted!\n");
    select_rows_example(storage, &table, 1);
    printf("Selected!\n");
    update_rows_example(storage, &table);
    printf("Updated!\n");
    remove_rows_example(storage, &table);
    printf("Removed!\n");

    // close table
    close_table(&table);
    close_storage(storage);
    return 0;
}

void insert_rows_example(Storage* storage, OpenedTable* table) {
    float f = 234.234f;
    uint16_t ui = 1;
    char* s = "<string! for tests!>";
    for (int i = 0; i < 100; i++) {
        void *row[] = {&f, &i, s, &ui};
        table_insert_row(storage, table, row);
        ui++;
        f += 1.2132f;
    }
}

void select_rows_example(Storage* storage, OpenedTable* table, uint8_t with_filter) {
    RowsIterator* iter = create_rows_iterator(storage, table);
    float bound = 240;
    if (with_filter)
        rows_iterator_add_filter(iter, greater_filter, &bound, "one");  // optional filter
    while (rows_iterator_next(iter) == REQUEST_ROW_FOUND) {
        printf("%f, %d, %s, %hu\n", *(float*)iter->row[0], *(int*)iter->row[1],
               (char*)iter->row[2], *(uint16_t *)iter->row[0]);
    }
    rows_iterator_free(iter);
}

void update_rows_example(Storage* storage, OpenedTable* table) {
    RowsIterator* iter = create_rows_iterator(storage, table);
    float f = 10.f;
    while (rows_iterator_next(iter) == REQUEST_ROW_FOUND) {
        void *row[] = {&f, 0, 0, 0};    // update only non-null fields
        rows_iterator_update_current(iter, row);
    }
    rows_iterator_free(iter);
}

void remove_rows_example(Storage* storage, OpenedTable* table) {
    RowsIterator* iter = create_rows_iterator(storage, table);
//    rows_iterator_add_filter(iter, equals_filter, &i, "four");  // optional filter
    while (rows_iterator_next(iter) == REQUEST_ROW_FOUND) {
        rows_iterator_remove_current(iter);
    }
    rows_iterator_free(iter);
}
