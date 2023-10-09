//
// Created by vlad on 28.09.23.
//
#include "defs.h"

#ifndef LAB1_PAGE_H
#define LAB1_PAGE_H

// in the file it stores like:
// row_size (4bytes), scale (2bytes), next(4bytes),
// and bitmap of rows occupancy
typedef struct PageMeta {
    uint32_t offset;
    // real file data
    uint32_t row_size;
    uint16_t scale;
    uint32_t next;
} PageMeta;

typedef enum RowReadStatus {
    READ_ROW_OK,
    READ_ROW_OUT_OF_BOUND,
    READ_ROW_IS_NULL,
    DEST_NOT_FREE
} RowReadStatus;

typedef enum RowWriteResult {
    WRITE_ROW_OK,
    WRITE_BITMAP_ERROR,
    WRITE_ROW_NOT_EMPTY,
    WRITE_ROW_OUT_OF_BOUND
} RowWriteResult;

typedef struct PageRow {
    uint32_t index;
    uint32_t size;
    void* data;
} PageRow;

uint32_t page_data_space(uint32_t row_size, uint16_t page_scale);

#endif //LAB1_PAGE_H
