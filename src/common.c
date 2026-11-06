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
