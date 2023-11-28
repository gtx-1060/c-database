//
// Created by vlad on 09.10.23.
//

#include <malloc.h>
#include <string.h>
#include <stdlib.h>
#include "table.h"
#include "util.h"
#include "page.h"

static uint8_t table_field_type_sizes[] = {
        [TABLE_FTYPE_INT_32] = 4,
        [TABLE_FTYPE_UINT_32] = 4,
        [TABLE_FTYPE_UINT_16] = 2,
        [TABLE_FTYPE_FLOAT] = 4,
        [TABLE_FTYPE_STRING] = 8,       // sz = uint32 page_number + uint32 row_number
        [TABLE_FTYPE_BOOL] = 1,
        [TABLE_FTYPE_BYTE] = 1,
//        [TABLE_FTYPE_FOREIGN_KEY] = 8,   // sz = uint32 page_number + uint32 row_number
        [TABLE_FTYPE_CHARS] = 0,          // must be set by hands
};

TableScheme create_table_scheme(uint16_t fields_n) {
    TableScheme scheme = {
            .capacity = fields_n,
            .size = 0,
            .fields = malloc(fields_n*sizeof(SchemeItem)),
    };
    if (scheme.fields == NULL)
        panic("CANNOT MALLOC SCHEME FIELDS ARRAY", 3);
    return scheme;
}

void add_scheme_field(TableScheme* scheme, const char* name, TableDatatype type, uint8_t nullable) {
    if (scheme->size >= scheme->capacity) {
        panic("SCHEME CAPACITY LESS THAN SIZE", 3);
    }
    SchemeItem* field = scheme->fields + scheme->size;
    field->type = type;
    field->nullable = nullable;
    field->order = scheme->size;
    field->name = malloc(strlen(name));
    field->actual_size = table_field_type_sizes[type];
    strcpy(field->name, name);
    scheme->size++;
}

void set_last_field_size(TableScheme* scheme, uint32_t actual_size) {
    if (scheme->size == 0) {
        panic("SCHEME SIZE IS 0, BUT ATTEMPT TO SET SIZE OF FIELD",3);
    }
    scheme->fields[scheme->size-1].actual_size = actual_size;
}

Table* init_table(const TableScheme* scheme, const char* name) {
    if (scheme->size != scheme->capacity)
        return NULL;
    Table* table = malloc(sizeof(Table));
    strncpy(table->name, name, TABLE_NAME_MAX_SZ);
    table->fields = scheme->fields;
    table->fields_n = scheme->capacity;
    table->row_size = 0;
    for (SchemeItem* field = table->fields; field < table->fields + table->fields_n; field++) {
        if  (field->type == 0 || strlen(field->name) == 0)
            panic("INCORRECT SCHEME DATA", 3);          // TODO: USE OPERATION STATUS INSTEAD OF PANIC
        table->row_size += field->actual_size;
    }
    uint16_t scale;
    for (scale = 1; page_actual_size(table->row_size, scale)
                    < table->row_size * TABLE_MIN_ROWS_PER_PAGE; scale++) ;
    table->page_scale = scale;
    return table;
}

static int scheme_cmp(const void* elem1, const void* elem2) {
    SchemeItem* it1 = (SchemeItem*)elem1;
    SchemeItem* it2 = (SchemeItem*)elem2;
    if (it1->order > it2->order) return  1;
    if (it1->order < it2->order) return -1;
    return 0;
}

void order_scheme_items(SchemeItem* array, size_t len) {
    qsort(array, len, sizeof(SchemeItem), scheme_cmp);
}

void free_scheme(SchemeItem* scheme, size_t length) {
    for (SchemeItem* item = scheme; item < item+length; item++) {
        if (item->name)
            free(item->name);
    }
    free(scheme);
}

// free table memory with scheme
void destruct_table(Table* table) {
    free_scheme(table->fields, table->fields_n);
    free(table);
}
