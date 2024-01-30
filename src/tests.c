#include <time.h>
#include <stdio.h>
#include "storage.h"
#include "search_filters.h"
#include "storage_intlzr.h"

static void insert_n_rows(Storage* storage, OpenedTable* table1, int t_number);
static void delete_random(Storage* storage, OpenedTable* table1,float chance);

__attribute__((unused)) static void test_insert_delete(Storage* storage, OpenedTable* table1);

static char* s = "test string 123123 test string 123123 test string 123123 ";

int main(void) {
    Storage* storage = init_storage("/home/vlad/Music/dbfile");
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
    }
    close_table(&table);
    close_storage(storage);
    return 0;
}

__attribute__((unused)) static void test_delete(Storage* storage, OpenedTable* table1) {
    clock_t t1;
    RowsIterator* iter;
    for (int i = 20 ; i < 200; i++) {
        insert_n_rows(storage, table1, i*10);
        t1 = clock();
        iter = create_rows_iterator(storage, table1);
        while (rows_iterator_next(iter) == REQUEST_ROW_FOUND) {
            rows_iterator_remove_current(iter);
        }
        printf("%f, ", (float)(clock()-t1) /  CLOCKS_PER_SEC);
        rows_iterator_free(iter);
    }
}

__attribute__((unused)) static void test_insert_delete(Storage* storage, OpenedTable* table1) {
    FILE* insert_test = fopen("/home/vlad/Music/insert.csv", "w+");
    FILE* remove_test = fopen("/home/vlad/Music/remove.csv", "w+");
    clock_t t1;
    for (int i = 0; i < 1000; i++) {
        for (int j = 0; j < 5; j++) {
            t1 = clock();
            insert_n_rows(storage, table1, 100);
            fprintf(insert_test, "%f, ", (float)(clock() - t1) / CLOCKS_PER_SEC);
        }
        for (int j = 0; j < 4; j++) {
            t1 = clock();
            delete_random(storage, table1, (float)100/(float)(500+(i*100)-(j*100)));
            fprintf(remove_test, "%f, ", (float)(clock() - t1) / CLOCKS_PER_SEC);
        }
    }
    fclose(insert_test);
    fclose(remove_test);
}

static void delete_random(Storage* storage, OpenedTable* table1,float chance) {
    RowsIterator* iter = create_rows_iterator(storage, table1);
    rows_iterator_add_filter(iter, random_filter, &chance, "four");
    int counter = 0;
    while (rows_iterator_next(iter) == REQUEST_ROW_FOUND) {
        rows_iterator_remove_current(iter);
        counter++;
    }
    printf("chance %f removed %d\n", chance, counter);
    rows_iterator_free(iter);
}

static void insert_n_rows(Storage* storage, OpenedTable* table1, int t_number) {
    float f = 234.234f;
    uint16_t ui = 1;
    for (int i = 0; i < t_number; i++) {
        void *row[] = {&f, &i, s, &ui};
        ui++;
        table_insert_row(storage, table1, row);
    }
}
