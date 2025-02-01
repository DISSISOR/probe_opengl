#ifndef arena_h_INCLUDED
#define arena_h_INCLUDED

#include "common.h"
#include "poly_allocator.h"

typedef struct Arena {
    u8 *buf;
    size_t prev_offset;
    size_t offset;
    size_t size;
} Arena;

void arena_init(Arena *arena, u8 *buf, size_t buf_size);
void arena_clear(Arena *arena);
void* arena_alloc(Arena *arena, size_t size, size_t alignment);
void* arena_realloc(Arena *arena, void *old_mem, size_t old_size, size_t new_size, size_t alignment);

static inline void* arena_alloc_opaque(void* arena, size_t size, size_t alignment) {
    return arena_alloc(arena, size, alignment);
}

static inline void arena_clear_opaque(void* arena) {
    arena_clear(arena);
}

static inline void* arena_realloc_opaque(void* arena, void *old_mem, size_t old_size, size_t new_size, size_t alignment) {
    return arena_realloc(arena, old_mem, old_size, new_size, alignment);
}

static inline void arena_free_opaque(void* arena, size_t size, size_t alignment) {
    UNUSED(arena);
    UNUSED(size);
    UNUSED(alignment);
}

#define ARENA_VTABLE { .alloc = arena_alloc_opaque,\
                       .realloc = arena_realloc_opaque,\
                       .free = arena_free_opaque,\
                       .clear = arena_free_opaque }
#define ARENA_POLY(arena) { .vtable = ARENA_VTABLE, .ctx = arena }

#define ARENA_MAKE_3(arena, type, count) ((type*)(arena_alloc(arena, sizeof(type) * (count), alignof(type))))
#define ARENA_MAKE_2(arena, type) ((type*)(arena_alloc(arena, sizeof(type), alignof(type))))
#define ARENA_MAKE(...) (CAT(ARENA_MAKE_, NARGS(__VA_ARGS__)) (__VA_ARGS__) )

#endif // arena_h_INCLUDED

