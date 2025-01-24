#ifndef common_h_INCLUDED
#define common_h_INCLUDED

#include <stdint.h>
#include <stdlib.h>

typedef uint8_t  u8;
typedef int8_t   i8;
typedef uint16_t u16;
typedef int16_t  i16;
typedef uint32_t u32;
typedef int32_t  i32;
typedef uint64_t u64;
typedef int64_t  i64;

typedef float    f32;
typedef double   f64;

#include <SDL3/SDL.h>
#define MY_ASSERT SDL_assert
#ifndef MY_ASSERT
#include <assert.h>
#define MY_ASSERT assert
#endif

#define TODO(text) MY_ASSERT(0 && "" text "")
#define UNREACHABLE(text) MY_ASSERT(0 && "" text "")

#define CAT_(x, y) x##y
#define CAT(x, y) CAT_(x, y)

#define STR_(x) #x
#define STR(x) STR(x)

#define NARGS_(_8, _7, _6, _5, _4, _3, _2, _1, n, ...) n
#define NARGS(...) NARGS_(__VA_ARGS__, 8, 7, 6, 5, 4, 3, 2, 1)
static_assert(5 == NARGS(1, 2, 3, 4, 5));

#define UNUSED(val) (void)(val)
#define ARRAY_LEN(ARR) ((ssize_t)(sizeof(ARR) / sizeof(*(ARR))))

#define CONTAINER_OF(ptr, type, mem) ((type*)((char*)ptr - (char*)offsetof(type, mem)))

#define COMMON_SIGNED_INT_TYPES_X(X)\
    X(i8)\
    X(i16)\
    X(i32)\
    X(i64)

#define COMMON_UNSIGNED_INT_TYPES_X(X)\
    X(u8)\
    X(u16)\
    X(u32)\
    X(u64)

#define COMMON_INT_TYPES_X(X)\
    COMMON_SIGNED_INT_TYPES_X(X)\
    COMMON_UNSIGNED_INT_TYPES_X(X)

#define COMMON_FLOAT_TYPES_X(X)\
    X(f32)\
    X(f64)

#define COMMON_NUM_TYPES_X(X)\
    COMMON_INT_TYPES_X(X)\
    COMMON_FLOAT_TYPES_X(X)

#define CLAP_GET(TYPE) static inline TYPE clap_##TYPE(TYPE min, TYPE max, TYPE inp) { \
    MY_ASSERT(min <= max); \
    inp = (min < inp) ? inp : min; \
    inp = (max > inp) ? inp : max; \
    return inp;\
}
COMMON_NUM_TYPES_X(CLAP_GET)
#undef CLAP_GET

#define CLAP_GET_FUNC_(x), x: clap_ ## x
#define CLAP_GET_FUNC(x) _Generic( (x) COMMON_NUM_TYPES_X(CLAP_GET_FUNC_) )

#define clap(min, max, inp) (CLAP_GET_FUNC(inp)((min), (max), (inp)))

typedef struct StringView {
    char *data;
    u32 size;
} StringView;
static_assert(sizeof(StringView) == 16);

static inline bool is_power_of_two(u64 x) {
    return (x & (x-1)) == 0;
}

static inline uintptr_t align_forward(uintptr_t addr, size_t align) {
    MY_ASSERT(is_power_of_two(align));
    uintptr_t ret_val = addr;
    uintptr_t modulo = addr & (align-1);
    if (modulo) {
        ret_val += align - modulo;
    }
    return ret_val;
}

#endif // common_h_INCLUDED
