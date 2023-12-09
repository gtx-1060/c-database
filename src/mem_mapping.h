//
// Created by vlad on 01.10.23.
//

#ifndef LAB1_MEM_MAPPING_H
#define LAB1_MEM_MAPPING_H

#include "defs.h"
#include "linked_list.h"

#if (defined(_WIN32) || defined(_WIN64)) && !defined(WIN)

    #define WIN
    #define MAP_FAILED NULL
    #include "windows.h"
    #include "winbase.h"
    #define mem_map(addr, length, handle, offset) win_mem_map(length, offset, handle)
    #define mem_unmap(addr, length) !UnmapViewOfFile(addr)

#elif defined(__unix__) || defined(__linux__)
#include <unistd.h>
#include "sys/mman.h"
#include "linked_list.h"

#define UNIX
#define ALLOC_GRAN 16

#define mem_unmap(addr, length) munmap(addr, length)
    #define resize_mapping(manager, offset, size) \
    posix_fallocate(manager->file_descriptor, offset*SYS_PAGE_SIZE, size*SYS_PAGE_SIZE);
    #ifdef BUILD_64
        #define mem_map(addr, length, fd, offset) \
        mmap(addr, length*SYS_PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, offset*SYS_PAGE_SIZE)
    #else
        #define mem_map(addr, length, prot, flags, fd, offset) \
        mmap2(addr, length, prot, flags, fd, offset)
    #endif
#endif

typedef struct Chunk {
    struct List list;
    uint16_t id;
    uint32_t size;          // in page units
    uint32_t offset;        // in page units
    uint16_t refs;
    void* pointer;
} Chunk;

typedef struct UserChunk {
    Chunk* backing_chunk;
    uint32_t size;          // in page units
    uint32_t offset;        // in page units
//    void* pointer;
} UserChunk;

typedef struct MemoryManager {
    Chunk* chunk_list;
    uint16_t chunks_ind;
    int file_descriptor;
    void* win_map_handle;       // used only on win
    uint32_t file_size;         // in pages; used only on win
    uint32_t alloc_gran;         // used only on win;
    Chunk* last_used;
} MemoryManager;

void* get_pages(MemoryManager* manager, uint32_t offset, uint32_t pages);
void destroy_memory_manager(MemoryManager* manager);
MemoryManager init_memory_manager(int fd);
UserChunk load_chunk(MemoryManager* manager, uint32_t offset, uint32_t pages);
void remove_chunk(MemoryManager* manager, UserChunk* chunk);
char* chunk_get_pointer(UserChunk* chunk);

#endif //LAB1_MEM_MAPPING_H
