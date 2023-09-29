//
// Created by vlad on 29.09.23.
//
#include <stdio.h>
#include "defs.h"


#ifndef LAB1_FILE_H
#define LAB1_FILE_H

struct FileHeader {
    uint32_t  magic_number;
    uint32_t page_size;
    uint32_t pages_number;
    uint16_t data_offset;
};

struct File {
    char* filename;
    struct FileHeader *header;
    FILE* data;
};

#endif //LAB1_FILE_H
