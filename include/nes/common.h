#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "nes/log.h"

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
#define get_bits(value, start, end) (((value) >> (start)) & ((1 << ((end) - (start) + 1)) - 1))
#define bit_mask(start, end) (((1 << ((end) - (start) + 1)) - 1) << (start))
#define mask_bits(value, start, end) ((value) & bit_mask(start, end))
#define clear_bits(value, start, end) ((value) & ~bit_mask(start, end))
#define copy_bits(dst, src, start, end) (((dst) & ~bit_mask(start, end)) | ((src) & bit_mask(start, end)))
#define nibble_low(value) ((value) & 0xF)
#define nibble_high(value) (((value) >> 4) & 0xF)

#define UNREACHABLE() assert(false && "Unreachable")
#define UNIMPLEMENTED() do { \
    fprintf(stderr, "'%s' not implemented!\n", __func__); \
    abort(); \
} while (0)

#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))

#define kib(value) (value << 10)

#define HEX_U8 "$%02X"
#define HEX_U16 "$%04X"

// =============================================================================
// Memory Bus
// =============================================================================

typedef struct MemoryBus MemoryBus;
struct MemoryBus {
    u8 (*read)(MemoryBus* bus, u16 address);
    void (*write)(MemoryBus* bus, u16 address, u8 value);
    void* ctx;
};

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

// =============================================================================
// Slice U8
// =============================================================================

typedef struct SliceU8 SliceU8;
struct SliceU8 {
    u8* data;
    u32 size;
};

extern SliceU8 slice_u8_from_parts(u8* data, u32 size);
extern SliceU8 slice_u8_subslice(SliceU8 slice, u32 offset, u32 length);
extern u8 slice_u8_get(SliceU8 slice, u32 index);
extern u8* slice_u8_get_ptr(SliceU8 slice, u32 index);

#endif // COMMON_H
