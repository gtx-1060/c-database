//
// Created by vlad on 09.10.23.
//

#include <malloc.h>
#include <string.h>
#include <stdlib.h>
#include "table.h"
#include "util.h"
#include "page.h"

Table* init_table(const TableScheme* scheme, const char* name) {
    if (scheme->size != scheme->capacity)
        return NULL;
    Table* table = malloc(sizeof(Table));
    table->name = strdup(name);
    table->fields = scheme->fields;
    table->fields_n = scheme->capacity;
    table->row_size = 0;
    for (SchemeItem* field = table->fields; field < table->fields + table->fields_n; field++) {
        if  (field->type == 0 || strlen(field->name) == 0)
            panic("INCORRECT SCHEME DATA", 3);          // TODO: USE OPERATION STATUS INSTEAD OF PANIC
        table->row_size += field->max_sz;
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

// free table memory with scheme
void destruct_table(Table* table) {
    free_scheme(table->fields, table->fields_n);
    free(table->name);
    free(table);
}

// PREDEFINED SCHEMES \/

TableScheme get_table_of_tables_scheme(void) {
    TableScheme scheme = create_table_scheme(7);
    add_scheme_field(&scheme, "name", TABLE_FTYPE_CHARS, 0);
    set_last_field_size(&scheme, 32);
    add_scheme_field(&scheme, "table_id", TABLE_FTYPE_UINT_16, 0);
    add_scheme_field(&scheme, "fields_n", TABLE_FTYPE_UINT_16, 0);
    add_scheme_field(&scheme, "page_scale", TABLE_FTYPE_UINT_16, 0);
    add_scheme_field(&scheme, "row_size", TABLE_FTYPE_UINT_32, 0);
    add_scheme_field(&scheme, "first_free_pg", TABLE_FTYPE_UINT_32, 0);
    add_scheme_field(&scheme, "first_full_pg", TABLE_FTYPE_UINT_32, 0);
    return scheme;
}

TableScheme get_scheme_table_scheme(void) {
    TableScheme scheme = create_table_scheme(5);
    add_scheme_field(&scheme, "name", TABLE_FTYPE_STRING, 0);
    add_scheme_field(&scheme, "type", TABLE_FTYPE_BYTE, 0);
    add_scheme_field(&scheme, "nullable", TABLE_FTYPE_BOOL, 0);
    add_scheme_field(&scheme, "order", TABLE_FTYPE_UINT_16, 0);
    add_scheme_field(&scheme, "table_id", TABLE_FTYPE_UINT_16, 0);
    return scheme;
}

TableScheme get_heap_table_scheme(uint32_t str_len) {
    TableScheme scheme = create_table_scheme(3);
    add_scheme_field(&scheme, "size", TABLE_FTYPE_UINT_16, 0);
    add_scheme_field(&scheme, "data", TABLE_FTYPE_CHARS, 1);
    set_last_field_size(&scheme, get_nearest_heap_size(str_len));
    add_scheme_field(&scheme, "table_id", TABLE_FTYPE_UINT_16, 0);
    return scheme;
}

uint16_t get_nearest_heap_size(uint32_t str_len) {
    return ((str_len / HEAP_ROW_SIZE_STEP) + (str_len % HEAP_ROW_SIZE_STEP > 0)) * HEAP_ROW_SIZE_STEP;
}
