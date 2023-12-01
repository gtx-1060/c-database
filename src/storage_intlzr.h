//
// Created by vlad on 23.11.23.
//

#ifndef LAB1_STORAGE_INTLZR_H
#define LAB1_STORAGE_INTLZR_H

#include "storage.h"


Storage* init_storage(char* filename);
void close_storage(Storage* storage);

#endif //LAB1_STORAGE_INTLZR_H
