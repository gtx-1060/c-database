//
// Created by vlad on 07.10.23.
//

#include <stdio.h>
#include <stdlib.h>
#include "util.h"


void panic(char* msg, int code) {
    perror(msg);
    exit(code);
}
