#include "nes/common.h"

#include <errno.h>
#include <string.h>

b8 read_file(const char* filename, u8* output_buffer, u32* length) {
    FILE* fp = fopen(filename, "rb");
    if (fp == NULL) {
        log_error("Failed to read file '%s': %s", filename, strerror(errno));
        return false;
    }

    fseek(fp, 0, SEEK_END);
    size_t _length = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (output_buffer == NULL) {
        *length = _length;
        return true;
    }

    if (_length > *length) {
        log_error("ERR: Too small buffer to read into.");
        return false;
    }

    *length = _length;
    fread(output_buffer, _length, sizeof(u8), fp);
    fclose(fp);

    return true;
}

// =============================================================================
// Slice U8
// =============================================================================

SliceU8 slice_u8_from_parts(u8* data, u32 size) {
    assert(data != NULL);
    return (SliceU8) {
        .data = data,
        .size = size,
    };
}

SliceU8 slice_u8_subslice(SliceU8 slice, u32 offset, u32 length) {
    assert(slice.data != NULL);
    assert(slice.size >= offset + length);
    return (SliceU8) {
        .data = slice.data + offset,
        .size = length,
    };
}

u8 slice_u8_get(SliceU8 slice, u32 index) {
    assert(slice.data != NULL);
    assert(index < slice.size);
    return slice.data[index];
}

u8* slice_u8_get_ptr(SliceU8 slice, u32 index) {
    assert(slice.data != NULL);
    assert(index < slice.size);
    return &slice.data[index];
}
