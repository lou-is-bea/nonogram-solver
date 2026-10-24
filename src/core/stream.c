#include "nonogram/stream.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "nonogram/status.h"

static bool read_arguments_are_valid(const void *ctx, const char *buf, size_t cap,
                                     const size_t *got)
{
    return ctx != nullptr && buf != nullptr && cap != 0u && got != nullptr;
}

/* --- stdio stream ------------------------------------------------------------------------- */

static ng_status file_read(void *ctx, char *buf, size_t cap, size_t *got)
{
    if (got != nullptr) {
        *got = 0;
    }
    if (!read_arguments_are_valid(ctx, buf, cap, got)) {
        return NG_ERR_INVALID_ARGUMENT;
    }
    FILE *file = ctx;
    const size_t count = fread(buf, 1, cap, file);
    /* A short count is either the end of the file or an error; only ferror tells them apart. */
    if (count < cap && ferror(file) != 0) {
        return NG_ERR_IO;
    }
    *got = count;
    return NG_OK;
}

static ng_status file_write(void *ctx, const char *buf, size_t len)
{
    if (ctx == nullptr || (buf == nullptr && len != 0u)) {
        return NG_ERR_INVALID_ARGUMENT;
    }
    if (len == 0u) {
        return NG_OK;
    }
    if (fwrite(buf, 1, len, (FILE *)ctx) != len) {
        return NG_ERR_IO;
    }
    return NG_OK;
}

ng_reader ng_reader_from_file(FILE *file)
{
    return (ng_reader){.read = file_read, .ctx = file};
}

ng_writer ng_writer_from_file(FILE *file)
{
    return (ng_writer){.write = file_write, .ctx = file};
}

/* --- memory block ------------------------------------------------------------------------- */

static ng_status memory_read(void *ctx, char *buf, size_t cap, size_t *got)
{
    if (got != nullptr) {
        *got = 0;
    }
    if (!read_arguments_are_valid(ctx, buf, cap, got)) {
        return NG_ERR_INVALID_ARGUMENT;
    }
    ng_memory_source *source = ctx;
    if (source->data == nullptr) {
        /* Only an empty block may have no data: it is at its end from the start. */
        return source->size == 0u ? NG_OK : NG_ERR_INVALID_ARGUMENT;
    }
    const size_t remaining = source->size - source->position;
    const size_t count = remaining < cap ? remaining : cap;
    if (count != 0u) {
        memcpy(buf, source->data + source->position, count);
        source->position += count;
    }
    *got = count;
    return NG_OK;
}

ng_reader ng_reader_from_memory(ng_memory_source *source, const char *data, size_t size)
{
    if (source != nullptr) {
        *source = (ng_memory_source){.data = data, .size = size, .position = 0};
    }
    return (ng_reader){.read = memory_read, .ctx = source};
}
