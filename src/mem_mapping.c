//
// Created by vlad on 06.10.23.
//
#include <stdio.h>
#include <malloc.h>
#include <fcntl.h>
#include "mem_mapping.h"
#include "util.h"


void unmap_chunk(Chunk* chunk);
void free_chunk(Chunk* chunk);

#if defined(WIN) && !defined(resize_mapping)

    HANDLE win_create_mapping(uint32_t pages, HANDLE filehandle) {
        uint64_t size = SYS_PAGE_SIZE * pages;
        return CreateFileMappingW(
                filehandle,
                NULL,
                PAGE_READWRITE,
                (uint32_t)(size >> 32),
                (uint32_t)size,
                NULL);
    }

    void* win_mem_map(uint32_t size, uint32_t offset, HANDLE handle) {
        uint64_t bytes_offset = (uint64_t)offset * SYS_PAGE_SIZE;
        return MapViewOfFile(
                handle,
                FILE_MAP_ALL_ACCESS,
                (uint32_t)(bytes_offset >> 32),
                (uint32_t)bytes_offset,
                (size_t)size*SYS_PAGE_SIZE
        );
    }

    void win_remap(MemoryManager* manager, uint32_t pages) {
        HANDLE old_mapping = manager->win_map_handle;
        HANDLE filehandle = (HANDLE)_get_osfhandle(manager->file_descriptor);
        manager->win_map_handle = win_create_mapping(pages, filehandle);
        if (manager->win_map_handle == NULL) {
            printf("winerr: %lu\n", GetLastError());
            panic("Cant re-map file", 1);
        }
        Chunk* chunk = manager->chunk_list;
        do {
            if (chunk->pointer != NULL) {
                mem_unmap(chunk->pointer, 0);
                chunk->pointer = mem_map(NULL, chunk->size, manager->win_map_handle, chunk->offset);
                if (chunk->pointer == MAP_FAILED) {
                    printf("winerr: %lu\n", GetLastError());
                    panic("CANNOT MAP CHUNK", 1);
                }
            }
            chunk = (Chunk*)chunk->list.next;
        } while (chunk->id != manager->chunk_list->id);
        if (old_mapping != NULL)
            CloseHandle(old_mapping);
    }

    #define resize_mapping(manager, offset, size) win_remap_if_necessary(manager, size, offset)
#endif

MemoryManager init_memory_manager(int fd) {
    MemoryManager manager;
    manager.file_size = 0;
    manager.file_descriptor = fd;
    manager.win_map_handle = NULL;
    manager.chunks_ind = 1;

    manager.chunk_list = malloc(sizeof(struct Chunk));
    manager.chunk_list->pointer = NULL;
    manager.chunk_list->offset = 0;
    manager.chunk_list->size = 0;
    manager.chunk_list->refs = 1;
    manager.chunk_list->id = manager.chunks_ind++;
    lst_init(&manager.chunk_list->list);
    manager.last_used = NULL;

#ifdef WIN
    SYSTEM_INFO inf;
    GetSystemInfo(&inf);
    manager.alloc_gran = inf.dwAllocationGranularity / SYS_PAGE_SIZE;
#else
    manager.alloc_gran = ALLOC_GRAN;
#endif

    return manager;
}

void destroy_memory_manager(MemoryManager* manager) {
    while (!lst_empty(&manager->chunk_list->list)) {
        Chunk* chunk = (Chunk*)manager->chunk_list->list.next;
        free_chunk(chunk);
    }
    unmap_chunk(manager->chunk_list);
    free(manager->chunk_list);
}

void unmap_chunk(Chunk* chunk) {
    if (chunk->pointer && mem_unmap(chunk->pointer, chunk->size * SYS_PAGE_SIZE)) {
        panic("CHNK UNMAP ERR", 1);
    }
    chunk->pointer = NULL;
}

void realloc_chunk(MemoryManager* manager, Chunk *chunk, uint32_t new_offset, uint32_t new_size) {
    unmap_chunk(chunk);
    uint32_t need_file_size = ((new_size+new_offset) / manager->alloc_gran + 1) * manager->alloc_gran;
    chunk->offset = (new_offset / manager->alloc_gran) * manager->alloc_gran;
    chunk->size = ((new_size+(new_offset-chunk->offset)) / manager->alloc_gran + 1) * manager->alloc_gran;
    #ifdef UNIX
    posix_fallocate(manager->file_descriptor, 0, need_file_size*SYS_PAGE_SIZE);
    chunk->pointer = (void*)mem_map(NULL, chunk->size, manager->file_descriptor, chunk->offset);
    #endif
    #ifdef WIN
    if (manager->file_size < need_file_size) {
        win_remap(manager, need_file_size);
        manager->file_size = need_file_size;
    }
    chunk->pointer = mem_map(NULL, chunk->size, manager->win_map_handle, chunk->offset);
    if (chunk->pointer == MAP_FAILED) {
        printf("winerr: %lu\n", GetLastError());
        panic("CANNOT MAP CHUNK", 1);
    }
    #endif
    if (chunk->pointer == MAP_FAILED) {
        panic("CANNOT MAP CHUNK", 1);
    }
}

Chunk* alloc_chunk(MemoryManager* manager, uint32_t offset, uint32_t pages) {
    Chunk* chunk = malloc(sizeof(struct Chunk));
    chunk->id = manager->chunks_ind++;
    chunk->pointer = NULL;
    chunk->refs = 1;
    lst_init(&chunk->list);
    lst_push(&manager->chunk_list->list, chunk);
    realloc_chunk(manager, chunk, offset, pages);
    return chunk;
}

Chunk* get_chunk_with(MemoryManager* manager, uint32_t offset, uint32_t pages) {
    Chunk* chunk = manager->chunk_list;
    do {
        if ((chunk->offset <= offset) && (chunk->size >= pages)
            && (chunk->size - pages >= offset - chunk->offset)) {
            chunk->refs++;
            return chunk;
        }
        chunk = (Chunk*)chunk->list.next;
    } while (chunk != manager->chunk_list);
    return alloc_chunk(manager, offset, pages);
}

UserChunk load_chunk(MemoryManager* manager, uint32_t offset, uint32_t pages) {
    UserChunk user_chunk;
    Chunk* parent = get_chunk_with(manager, offset, pages);
    user_chunk.backing_chunk = parent;
    user_chunk.offset = offset;
    user_chunk.size = pages;
    return user_chunk;
}

void free_chunk(Chunk* chunk) {
    if (chunk->refs == 0) {
        panic("chnk refs is 0!", 1);
    } else if (chunk->refs == 1) {
        unmap_chunk(chunk);
        lst_remove(&chunk->list);
        free(chunk);
        return;
    }
    chunk->refs--;
}

void remove_chunk(MemoryManager* manager, UserChunk* chunk) {
    free_chunk(chunk->backing_chunk);
}

void* get_pages(MemoryManager* manager, uint32_t offset, uint32_t pages) {
    if (manager->last_used != NULL) {
        free_chunk(manager->last_used);
    }
    manager->last_used = get_chunk_with(manager, offset, pages);
    return ((char*)manager->last_used->pointer) + (offset - manager->last_used->offset) * SYS_PAGE_SIZE;
}

char* chunk_get_pointer(UserChunk* chunk) {
    return ((char*)chunk->backing_chunk->pointer) + ((chunk->offset - chunk->backing_chunk->offset) * SYS_PAGE_SIZE);
}