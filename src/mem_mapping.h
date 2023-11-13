//
// Created by vlad on 01.10.23.
//

#ifndef LAB1_MEM_MAPPING_H
#define LAB1_MEM_MAPPING_H

#define DEFAULT_CHUNK_SIZE 10

#include "defs.h"
#if defined(_WIN32) || defined(_WIN64)
    // TODO: ADD MEM_MAP DEF
#elif defined(__unix__) || defined(__linux__)
    #include <unistd.h>
    #include "sys/mman.h"
#include "linked_list.h"

#define mem_unmap(addr, length) munmap(addr, length)
    #ifdef BUILD_64
        #define mem_map(addr, length, prot, flags, fd, offset) \
        mmap(addr, length, prot, flags, fd, offset*SYS_PAGE_SIZE)
    #else
        #define mem_map(addr, length, prot, flags, fd, offset) \
        mmap2(addr, length, prot, flags, fd, offset)
    #endif
#endif

typedef uint32_t chunk_id;

// TODO: make it possible to define size of chunk and possibility of reallocation
typedef struct Chunk {
    struct List list;
    uint16_t id;
    uint32_t size;          // in page units
    uint32_t offset;        // in page units
    uint8_t manual_control;
    void* pointer;
} Chunk;

typedef struct MemoryManager {
    Chunk* chunk_list;
    uint16_t chunks_n;
    int file_descriptor;
} MemoryManager;

void* get_pages(MemoryManager* manager, uint32_t offset, uint32_t pages);
void destroy_memory_manager(MemoryManager* manager);
MemoryManager init_memory_manager(int fd);
Chunk* load_chunk(MemoryManager* manager, uint32_t offset, uint32_t pages, uint8_t manual);
void remove_chunk(MemoryManager* manager, chunk_id id);

#endif //LAB1_MEM_MAPPING_H
