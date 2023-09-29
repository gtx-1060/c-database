cmake_minimum_required(VERSION 3.25)
project(lab1 C)

set(CMAKE_C_STANDARD 11)

add_executable(lab1 src/main.c src/page.h src/page.c src/defs.h src/file.h src/file.c)
