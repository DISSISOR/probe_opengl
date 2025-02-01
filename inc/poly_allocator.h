#ifndef poly_allocator_h_INCLUDED
#define poly_allocator_h_INCLUDED

#include <stddef.h>

#include "common.h"

typedef struct AllocatorVTable {
    void* (*alloc) (void *ctx, size_t size, size_t alignment);
    void* (*realloc) (void *ctx, void *old_mem, size_t old_size, size_t new_size, size_t alignment);
    void (*free) (void *ctx, void *mem, size_t size);
    void (*clear) (void *ctx);
} AllocatorVTable;

typedef struct AllocatorPoly {
    AllocatorVTable vtable;
    void *ctx;
} AllocatorPoly;

static inline void* allocator_poly_alloc(AllocatorPoly alloc, size_t size, size_t alignment) {
    return alloc.vtable.alloc(alloc.ctx, size, alignment);
}

static inline void* allocator_poly_realloc(AllocatorPoly alloc, void *old_mem, size_t old_size, size_t new_size, size_t alignment) {
    if (alloc.vtable.realloc)
        return alloc.vtable.realloc(alloc.ctx, old_mem, old_size, new_size, alignment);
    alloc.vtable.free(alloc.ctx, old_mem, old_size);
    return alloc.vtable.alloc(alloc.ctx, new_size, alignment);
}

static inline void allocator_poly_free(AllocatorPoly alloc, void *mem, size_t size) {
    alloc.vtable.free(alloc.ctx, mem, size);
}

static inline void allocator_poly_clear(AllocatorPoly alloc) {
    alloc.vtable.clear(alloc.ctx);
}


#endif // poly_allocator_h_INCLUDED
