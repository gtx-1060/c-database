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
    #define mem_unmap(addr, length) munmap(addr, length)
    #ifdef BUILD_64
        #define mem_map(addr, length, prot, flags, fd, offset) \
        mmap(addr, length, prot, flags, fd, offset*SYS_PAGE_SIZE)
    #else
        #define mem_map(addr, length, prot, flags, fd, offset) \
        mmap2(addr, length, prot, flags, fd, offset)
    #endif
#endif


// TODO: make it possible to define size of chunk and possibility of reallocation
typedef struct Chunk {
    // in page units
    uint32_t offset;
    // in page units
    uint32_t size;
    uint8_t can_realloc;
    void* data;
} Chunk;

typedef struct MemoryManager {
    Chunk chunk;
    int file_descriptor;
} MemoryManager;

void* get_mapped_pages(MemoryManager* manager, uint32_t offset, uint32_t pages);
void destruct_memory_manager(MemoryManager* manager);
MemoryManager* init_memory_manager(int fd, uint32_t chunk_size, uint8_t auto_realloc);

#endif //LAB1_MEM_MAPPING_H
