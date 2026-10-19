#include "nes/common.h"

#include <errno.h>
#include <string.h>

b8 read_file(const char* filename, u8* output_buffer, u32* length) {
    FILE* fp = fopen(filename, "rb");
    if (fp == NULL) {
        fprintf(stderr, "ERR: Failed to read file '%s': %s\n", filename, strerror(errno));
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
        fprintf(stderr, "ERR: Too small buffer to read into.\n");
        return false;
    }

    *length = _length;
    fread(output_buffer, _length, sizeof(u8), fp);
    fclose(fp);

    return true;
}
