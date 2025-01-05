#ifndef common_h_INCLUDED
#define common_h_INCLUDED

#include <assert.h>
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

#define MY_ASSERT assert

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

#define CLAP_GET(TYPE) static inline TYPE clap##TYPE(TYPE min, TYPE max, TYPE inp) { \
    MY_ASSERT(min <= max); \
    inp = (min < inp) ? inp : min; \
    inp = (max > inp) ? inp : max; \
    return inp;\
}
COMMON_NUM_TYPES_X(CLAP_GET)
#undef CLAP_GET

#define UNUSED(val) (void)(val)
#define ARRAY_LEN(ARR) ((ssize_t)(sizeof(ARR) / sizeof(*(ARR))))

typedef struct StringView {
    char *data;
    u32 size;
} StringView;
static_assert(sizeof(StringView) == 16);

#endif // common_h_INCLUDED
