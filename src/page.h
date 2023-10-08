//
// Created by vlad on 28.09.23.
//
#include "defs.h"

#ifndef LAB1_PAGE_H
#define LAB1_PAGE_H

#define DEFAULT_PG_SZ 4096

struct PageHeader {
    uint16_t row_size;
    uint32_t next_page;
};

struct Page {
    struct PageHeader header;
    void *data;
};

#endif //LAB1_PAGE_H
