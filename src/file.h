//
// Created by vlad on 29.09.23.
//
#include <stdio.h>
#include "defs.h"


#ifndef LAB1_FILE_H
#define LAB1_FILE_H

struct FileHeader {
    uint32_t magic_number;
    // in page units
    uint32_t pages_number;
    uint16_t data_offset;
};

struct File {
    char* filename;
    struct FileHeader *header;
    int descriptor;
};

#endif //LAB1_FILE_H
