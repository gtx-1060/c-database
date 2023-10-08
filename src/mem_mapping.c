//
// Created by vlad on 06.10.23.
//
#include <stdio.h>
#include <malloc.h>
#include "defs.h"
#include "mem_mapping.h"
#include "util.h"

#define DEFAULT_CHUNK_SIZE 50

// TODO: Maybe use queue of chunks for better performance


void unmap_chunk(Chunk* chunk);

MemoryManager* init_memory_manager(int fd) {
    MemoryManager* manager = malloc(sizeof(MemoryManager));
    if (manager == NULL)
        panic("Cannot create memory manager", 1);
    manager->chunk.data = NULL;
    return manager;
}

void destruct_memory_manager(MemoryManager* manager) {
    unmap_chunk(&manager->chunk);
    free(manager);
}

void unmap_chunk(Chunk* chunk) {
    if (chunk->data && mem_unmap(chunk->data, chunk->size*SYS_PAGE_SIZE) != 0) {
        panic("CHNK UNMAP ERR", 1);
    }
}

void realloc_chunk(MemoryManager* manager, uint32_t new_offset, uint32_t new_size) {
    unmap_chunk(&manager->chunk);
    manager->chunk.offset = new_offset;
    manager->chunk.size = new_size;
    manager->chunk.data = mem_map(0, new_size*SYS_PAGE_SIZE, PROT_READ || PROT_WRITE,
          MAP_SHARED, manager->file_descriptor, new_offset);
    if (manager->chunk.data == MAP_FAILED) {
        panic("CANNOT MAP CHUNK", 1);
    }
}

void* read_pages(MemoryManager* manager, uint32_t offset, uint32_t number) {
    Chunk chunk = manager->chunk;
    if (chunk.data != NULL && (offset > chunk.offset &&
       offset + number*SYS_PAGE_SIZE < chunk.offset + SYS_PAGE_SIZE*chunk.size)) {
        return (chunk.data) + (offset-chunk.offset)*SYS_PAGE_SIZE;
    }
    uint32_t size = (number > DEFAULT_CHUNK_SIZE) ? number : DEFAULT_CHUNK_SIZE;
    realloc_chunk(manager, offset, size);
    return chunk.data;
}


