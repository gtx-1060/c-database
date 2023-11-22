//
// Created by vlad on 06.10.23.
//
#include <stdio.h>
#include <malloc.h>
#include "defs.h"
#include "mem_mapping.h"
#include "util.h"


void unmap_chunk(Chunk* chunk);

MemoryManager init_memory_manager(int fd) {
    MemoryManager manager;
    manager.file_descriptor = fd;
    manager.chunks_n = 1;
    manager.chunk_list = malloc(sizeof(struct Chunk));
    lst_init(&manager.chunk_list->list);
    return manager;
}

void destroy_memory_manager(MemoryManager* manager) {
    while (!lst_empty(&manager->chunk_list->list)) {
        Chunk* chunk = (Chunk *) manager->chunk_list->list.next;
        remove_chunk(manager, chunk->id);
    }
    unmap_chunk(manager->chunk_list);
    free(manager->chunk_list);
}

void unmap_chunk(Chunk* chunk) {
    if (chunk->pointer && mem_unmap(chunk->pointer, chunk->size * SYS_PAGE_SIZE) != 0) {
        panic("CHNK UNMAP ERR", 1);
    }
}

void realloc_chunk(MemoryManager* manager, Chunk *chunk, uint32_t new_offset, uint32_t new_size) {
    unmap_chunk(chunk);
    chunk->offset = new_offset;
    chunk->size = new_size;
    chunk->pointer = mem_map(0, chunk->offset * SYS_PAGE_SIZE, PROT_READ || PROT_WRITE,
                                     MAP_SHARED, manager->file_descriptor, new_offset);
    if (chunk->pointer == MAP_FAILED) {
        panic("CANNOT MAP CHUNK", 1);
    }
}

void* get_chunk_pages(MemoryManager* manager, Chunk* chunk, uint32_t offset, uint32_t pages) {
    if (chunk->pointer != NULL && (offset > chunk->offset &&
               offset + pages * SYS_PAGE_SIZE < chunk->offset + SYS_PAGE_SIZE * chunk->size)) {
        return (chunk->pointer) + (offset - chunk->offset) * SYS_PAGE_SIZE;
    }
    if (chunk->manual_control) {
        panic("ATTEMPT TO GO OUT OF CHUNK BOUNDS!", 1);
    }
    uint32_t size = (pages > DEFAULT_CHUNK_SIZE) ? pages : DEFAULT_CHUNK_SIZE;
    realloc_chunk(manager, chunk, offset, size);
    return chunk->pointer;
}

// TODO: check an existence of identical chunk
Chunk* load_chunk(MemoryManager* manager, uint32_t offset, uint32_t pages, uint8_t manual) {
    Chunk* chunk = malloc(sizeof(struct Chunk));
    chunk->manual_control = manual;
    chunk->id = manager->chunks_n++;
    lst_init(&chunk->list);
    lst_push(&manager->chunk_list->list, chunk);
    realloc_chunk(manager, chunk, offset, pages);
    return chunk;
}

void remove_chunk(MemoryManager* manager, chunk_id id) {
    Chunk* chunk = (Chunk *)manager->chunk_list->list.next;
    while (chunk->id != id && chunk->id != 0) {
        chunk = (Chunk *)chunk->list.next;
    }
    if (chunk->id == id) {
        unmap_chunk(chunk);
        lst_remove(&chunk->list);
        free(chunk);
    }
}

void* get_pages(MemoryManager* manager, uint32_t offset, uint32_t pages) {
    return get_chunk_pages(manager, manager->chunk_list, offset, pages);
}
