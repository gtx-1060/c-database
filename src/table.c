//
// Created by vlad on 09.10.23.
//

#include <malloc.h>
#include <string.h>
#include "table.h"
#include "util.h"
#include "page.h"

static uint8_t table_field_type_sizes[] = {
        [TABLE_FTYPE_INT_32] = 4,
        [TABLE_FTYPE_FLOAT] = 4,
        [TABLE_FTYPE_STRING] = 8,       // sz = uint32 page_number + uint32 row_number
        [TABLE_FTYPE_BOOL] = 1,
        [TABLE_FTYPE_BYTE] = 1,
        [TABLE_FTYPE_FOREIGN_KEY] = 8   // sz = uint32 page_number + uint32 row_number
};

TableScheme create_table_scheme(uint16_t fields_n) {
    TableScheme scheme = {
            .capacity = fields_n,
            .size = 0,
            .fields = malloc(fields_n*sizeof(SchemeItem))
    };
    if (scheme.fields == NULL)
        panic("CANNOT MALLOC SCHEME FIELDS ARRAY", 3);
    return scheme;
}

void add_scheme_field(TableScheme* scheme, const char* name, TableDatatype type, uint8_t nullable) {
    SchemeItem* field = scheme->fields + scheme->size;
    field->type = type;
    field->nullable = nullable;
    field->name = malloc(strlen(name));
    strcpy(field->name, name);
    scheme->size++;
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
        table->row_size += table_field_type_sizes[field->type];
    }
    uint16_t scale;
    for (scale = 1; page_actual_size(table->row_size, scale)
                    < table->row_size * TABLE_MIN_ROWS_PER_PAGE; scale++) ;
    table->page_scale = scale;
    return table;
}

void destruct_table(Table* table) {
    for (SchemeItem* item = table->fields; item < table->fields+table->fields_n; item++) {
        if (item->name)
            free(item->name);
    }
    free(table->fields);
    free(table);
}
