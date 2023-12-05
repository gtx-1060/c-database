//
// Created by vlad on 07.10.23.
//

#ifndef LAB1_UTIL_H
#define LAB1_UTIL_H

#include <stdio.h>
#include <stdlib.h>
#include "defs.h"

enum TableActions {
    ROW_INSERT = 0,
    ROW_REMOVE = 1,
    ROW_REPLACE = 2,
    ROW_REMOVE_NOT_FULL = 3,
    ROW_REMOVE_FREED = 4,
    ROW_INSERT_FULL = 5
};

void tlog(enum TableActions action, char* tname, uint32_t page, uint32_t row);
void panic(char* msg, int code);

#endif //LAB1_UTIL_H
