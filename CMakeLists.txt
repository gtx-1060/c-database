cmake_minimum_required(VERSION 3.25)
project(database-from-scratch C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_COMPILER "gcc")
set(CMAKE_C_FLAGS "-O0")

add_library(db-api
        src/page.h src/page.c src/defs.h src/mem_mapping.h src/mem_mapping.c src/util.c src/util.h src/table.c src/table.h src/storage.c src/storage.h src/linked_list.h src/linked_list.c
        src/rows_iterator.h src/rows_iterator.c src/storage_intlzr.h src/storage_intlzr.c src/search_filters.h src/search_filters.c src/join.h src/join.c src/tests/tests.c src/scheme.c src/scheme.c
)
target_link_libraries(db-api PUBLIC m)
target_compile_options(db-api PUBLIC -Werror -Wall -Wextra -pedantic -Wmissing-prototypes -Wstrict-prototypes)

add_executable(usage-example src/examples/crud.c)
target_link_libraries(usage-example PRIVATE db-api)

# tests
add_executable(database-test src/tests/tests.c)
target_link_libraries(database-test PRIVATE db-api)
enable_testing()
add_test(
        NAME crud-test
        COMMAND $<TARGET_FILE:usage-example>
)
add_test(
        NAME consistency-test
        COMMAND $<TARGET_FILE:database-test>
)

# for coverage statistics
SET(GCC_COVERAGE_COMPILE_FLAGS -fprofile-arcs -ftest-coverage -fPIC)
target_compile_options(database-test PRIVATE ${GCC_COVERAGE_COMPILE_FLAGS})
target_link_libraries(database-test PRIVATE gcov)

#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -lm -ftest-coverage -fprofile-arcs")