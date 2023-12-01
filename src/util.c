//
// Created by vlad on 07.10.23.
//

#include "util.h"

char* log_phrases[] = {
        [ROW_INSERT] = "inserted",
        [ROW_REMOVE] = "removed",
        [ROW_REPLACE] = "replaced",
        [ROW_REMOVE_FREED] = "removed; page freed",
        [ROW_INSERT_FULL] = "inserted; page moved to full"
};

void panic(char* msg, int code) {
    perror(msg);
    exit(code);
}

void tlog(enum TableActions action, char* tname, uint32_t page, uint32_t row) {
    printf("[Log]   Record successfully %s        table='%s'      page=%u       row=%u\n",
           log_phrases[action], tname, page, row);
}