//
// Created by vlad on 10.01.24.
//

#include "scheme.h"

static uint8_t table_field_type_sizes[] = {
        [TABLE_FTYPE_INT_32] = 4,
        [TABLE_FTYPE_UINT_32] = 4,
        [TABLE_FTYPE_UINT_16] = 2,
        [TABLE_FTYPE_FLOAT] = 4,
        [TABLE_FTYPE_STRING] = 8,       // sz = uint32 page_number + uint32 row_number
        [TABLE_FTYPE_BOOL] = 1,
        [TABLE_FTYPE_BYTE] = 1,
        [TABLE_FTYPE_CHARS] = 0,        // must be set by hands
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

void insert_scheme_field(TableScheme* scheme, const char* name, TableDatatype type,
                         uint8_t nullable, uint16_t index) {
    if (index >= scheme->capacity) {
        panic("SCHEME CAPACITY LESS THAN INSERT INDEX", 3);
    }
    SchemeItem* field = scheme->fields + index;
    field->type = type;
    field->nullable = nullable;
    field->order = index;
    field->name = strdup(name);
    field->max_sz = table_field_type_sizes[type];
}

void add_scheme_field(TableScheme* scheme, const char* name, TableDatatype type, uint8_t nullable) {
    if (scheme->size >= scheme->capacity) {
        panic("SCHEME CAPACITY LESS THAN SIZE", 3);
    }
    insert_scheme_field(scheme, name, type, nullable, scheme->size);
    scheme->size++;
}

void set_last_field_size(TableScheme* scheme, uint16_t actual_size) {
    if (scheme->size == 0) {
        panic("SCHEME SIZE IS 0, BUT ATTEMPT TO SET SIZE OF FIELD",3);
    }
    scheme->fields[scheme->size-1].max_sz = actual_size;
}

void free_scheme(SchemeItem* scheme, size_t length) {
    for (uint32_t i = 0; i < length; i++) {
        if (scheme[i].name)
            free(scheme[i].name);
    }
    free(scheme);
}
