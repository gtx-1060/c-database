//
// Created by vlad on 28.09.23.
//
#include "defs.h"

#ifndef LAB1_PAGE_H
#define LAB1_PAGE_H

struct PageHeader {
    uint32_t next_page;
    // ...
};


struct Page {
    struct PageHeader *header;
    uint8_t *data;
};

#endif //LAB1_PAGE_H
