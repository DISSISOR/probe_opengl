#ifndef arena_h_INCLUDED
#define arena_h_INCLUDED

#include "common.h"

typedef struct Arena {
    u8 *buf;
    u32 prev_offset;
    u32 offset;
    u32 size;
} Arena;

void arena_init(Arena *arena, u8 *buf, u32 buf_size);
void arena_free(Arena *arena);
void* arena_alloc(Arena *arena, u32 size, u32 alignment);
void* arena_realloc(Arena *arena, void *old_mem, u32 old_size, u32 new_size, u32 alignment);

#define ARENA_MAKE_3(arena, type, count) ((type*)(arena_alloc(arena, sizeof(type) * (count), alignof(type))))
#define ARENA_MAKE_2(arena, type) ((type*)(arena_alloc(arena, sizeof(type), alignof(type))))
#define ARENA_MAKE(...) (CAT(ARENA_MAKE_, NARGS(__VA_ARGS__))(__VA_ARGS__))

#endif // arena_h_INCLUDED

