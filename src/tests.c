#include <time.h>
#include <stdio.h>
#include "storage.h"
#include "search_filters.h"
#include "util.h"

void insert_n_rows(Storage* storage, OpenedTable* table1, int t_number);
void delete_random(Storage* storage, OpenedTable* table1,float chance);

static char* s = "test string 123123 test string 123123 test string 123123 ";

void test_delete(Storage* storage, OpenedTable* table1) {
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

void test_insert_delete(Storage* storage, OpenedTable* table1) {
    clock_t t1;
    clock_t* tests = malloc(sizeof(clock_t) * 9 * 20);
    clock_t* p = tests;
    for (int i = 0; i < 20; i++) {
        for (int j = 0; j < 5; j++) {
            t1 = clock();
            insert_n_rows(storage, table1, 100);
            *p = clock() - t1;
            p++;
        }
        for (int j = 0; j < 4; j++) {
            t1 = clock();
            delete_random(storage, table1, (float)100/(float)(500+(i*100)-(j*100)));
            *p = clock() - t1;
            p++;
        }
    }

    for (p = tests; p < tests + 9 * 20; p++) {
        printf("%f, ", (float)(*p) /  CLOCKS_PER_SEC);
    }
    free(tests);
}

void delete_random(Storage* storage, OpenedTable* table1,float chance) {
    RowsIterator* iter = create_rows_iterator(storage, table1);
    rows_iterator_add_filter(iter, random_filter, &chance, "four");
    int counter = 0;
    while (rows_iterator_next(iter) == REQUEST_ROW_FOUND) {
        rows_iterator_remove_current(iter);
        counter++;
    }
//    printf("chance %f removed %d\n", chance, counter);
    rows_iterator_free(iter);
}

void insert_n_rows(Storage* storage, OpenedTable* table1, int t_number) {
    float f = 234.234f;
    uint16_t ui = 1;
    for (int i = 0; i < t_number; i++) {
        void *row[] = {&f, &i, s, &ui};
        ui++;
        table_insert_row(storage, table1, row);
    }
}