cmake_minimum_required(VERSION 3.25)
project(lab1 C)

set(CMAKE_C_STANDARD 11)

add_executable(lab1 src/main.c
        src/page.h src/page.c src/defs.h src/mem_mapping.h src/mem_mapping.c src/util.c src/util.h src/table.c src/table.h src/storage.c src/storage.h src/linked_list.h src/linked_list.c
        src/rows_iterator.h src/rows_iterator.c src/storage_intlzr.h src/storage_intlzr.c src/search_filters.h src/search_filters.c src/join.h src/join.c src/tests.c src/tests.h src/scheme.c src/scheme.c)

# TO PERF PROFILE WORKS WELL
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -lm")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -lm")