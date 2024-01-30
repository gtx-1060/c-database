//
// Created by vlad on 07.10.23.
//

#ifndef LAB1_UTIL_H
#define LAB1_UTIL_H

#include <stdio.h>
#include <stdlib.h>
#include "defs.h"

#define LOG 0

enum TableActions {
    ROW_INSERT = 0,
    ROW_REMOVE = 1,
    ROW_REPLACE = 2,
    ROW_REMOVE_NOT_FULL = 3,
    ROW_REMOVE_FREED = 4,
    ROW_INSERT_FULL = 5
};

#if LOG
static char* log_phrases[] = {
        [ROW_INSERT] = "inserted",
        [ROW_REMOVE] = "removed",
        [ROW_REPLACE] = "replaced",
        [ROW_REMOVE_NOT_FULL] = "removed; page not full",
        [ROW_REMOVE_FREED] = "removed; page freed",
        [ROW_INSERT_FULL] = "inserted; page moved to full"
};

#define tlog(action, tname, page_id, row_id) \
do { \
    printf("[Log]   Record successfully %s        table='%s'      page=%u       row=%u\n", \
        log_phrases[action], tname, page_id, row_id);  \
} while (0)
#else
#define tlog(action, tname, page_id, row_id) ;
#endif

void panic(char* msg, int code);

#endif //LAB1_UTIL_H
