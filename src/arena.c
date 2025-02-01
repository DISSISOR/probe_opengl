#include "arena.h"

void arena_init(Arena *arena, u8 *buf, size_t buf_size) {
    // MY_ASSERT(is_power_of_two((uintptr_t)buf));
    MY_ASSERT(buf);
    MY_ASSERT(buf_size);
    arena->buf = buf;
    arena->prev_offset = 0;
    arena->offset = 0;
    arena->size = buf_size;
}

void arena_clear(Arena *arena) {
    arena->offset = 0;
    arena->prev_offset = 0;
}

void* arena_alloc(Arena *arena, size_t size, size_t alignment) {
    MY_ASSERT(is_power_of_two(alignment));
    arena->prev_offset = arena->offset;
    u32 offset = align_forward(arena->offset, alignment);
    if (arena->size - offset < size) {
        return NULL;
    }
    arena->offset = offset + size;
    return arena->buf + offset;
}

void* arena_realloc(Arena *arena, void *old_mem, size_t old_size, size_t new_size, size_t alignment) {
    MY_ASSERT(is_power_of_two(alignment));

	if ((u8*)old_mem < arena->buf || (u8*)old_mem > (arena->buf + arena->size)) {
        UNREACHABLE("Out of bounds of the arena");
        return NULL;
	}

    if (old_mem == NULL || old_size == 0) {
		return arena_alloc(arena, new_size, alignment);
	}

    if (old_mem == arena->buf + arena->prev_offset) {
        arena->offset += new_size - old_size;
        return old_mem;
    }
    void *ret_val = arena_alloc(arena, new_size, alignment);
    u32 copy_size = old_size < new_size ? old_size : new_size;
    memmove(ret_val, old_mem, copy_size);
    return ret_val;
}

