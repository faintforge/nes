#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

typedef int8_t  i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef float f32;
typedef double f64;

typedef u8  b8;
typedef u32 b32;

#ifndef true
#define true (1)
#endif // true

#ifndef false
#define false (0)
#endif // true

#ifndef NULL
#define NULL ((void*) 0)
#endif // NULL

#define get_bit(value, bit_index) (((value) >> (bit_index)) & 1)

#define UNREACHABLE() assert(false && "Unreachable")
#define UNIMPLEMENTED() do { \
    fprintf(stderr, "'%s' not implemented!\n", __func__); \
    abort(); \
} while (0)

#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))

// =============================================================================
// Color
// =============================================================================

typedef struct Color Color;
struct Color {
    u8 r, g, b;
};

#define color_arg(c) (c).r, (c).g, (c).b
#define color_arg_float(c) (float) (c).r, (float) (c).g, (float) (c).b

// =============================================================================
// File
// =============================================================================

extern b8 read_file(const char* filename, u8* output, u32* length);

#endif // COMMON_H
