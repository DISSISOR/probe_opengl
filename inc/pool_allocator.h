#ifndef pool_allocator_h_INCLUDED
#define pool_allocator_h_INCLUDED

#include "poly_allocator.h"
#include "common.h"

typedef struct PoolAllocator {
    AllocatorPoly backing_allocator;
    size_t elem_size;
    size_t elem_alignment;
    uintptr_t free_list;
} PoolAllocator;

static inline void pool_allocator_init(PoolAllocator *alloc, AllocatorPoly backing_allocator, size_t elem_size, size_t elem_alignment) {
    alloc->elem_size = elem_size;
    alloc->elem_alignment = elem_alignment;
    alloc->free_list = 0;
    alloc->backing_allocator = backing_allocator;
}

static inline void* pool_allocator_alloc(PoolAllocator *alloc) {
    if (!alloc->free_list) {
        return allocator_poly_alloc(alloc->backing_allocator, alloc->elem_size, 8);
    }
    void *ret_val = (void*)alloc->free_list;
    alloc->free_list = *( (uintptr_t*)alloc->free_list );
    return ret_val;
}

static inline void pool_allocator_free(PoolAllocator *alloc, void *mem) {
    *(uintptr_t*)mem = alloc->free_list;
    alloc->free_list = (uintptr_t)mem;
}

static inline void pool_allocator_clear(PoolAllocator *alloc) {
    allocator_poly_clear(alloc->backing_allocator);
    alloc->free_list = 0;
}

static inline void* pool_allocator_alloc_opaque(void *ctx, size_t size, size_t alignment) {
    PoolAllocator *const alloc = ctx;
    MY_ASSERT(size == alloc->elem_size);
    MY_ASSERT(alignment == alloc->elem_alignment);
    return pool_allocator_alloc(ctx);
}

static inline void pool_allocator_free_opaque(void *ctx, void *mem, size_t size, size_t alignment) {
    PoolAllocator *const alloc = ctx;
    MY_ASSERT(size == alloc->elem_size);
    MY_ASSERT(alignment == alloc->elem_alignment);
    pool_allocator_free(alloc, mem);
}

static inline void pool_allocator_clear_opaque(void *ctx) {
    pool_allocator_clear(ctx);
}

#define POOL_ALLOCATOR_VTABLE { .alloc = pool_allocator_alloc_opaque,\
                       .free = pool_allocator_free_opaque,\
                       .clear = pool_allocator_free_opaque }
#define POOL_ALLOCATOR_POLY(pool_allocator) { .vtable = POOL_ALLOCATOR_VTABLE, .ctx = pool_allocator }

#endif // pool_allocator_h_INCLUDED
