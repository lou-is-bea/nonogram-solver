#include "test_streams.h"

#include <stdint.h>
#include <string.h>

#include "nonogram/status.h"
#include "nonogram/stream.h"

static ng_status source_read(void *ctx, char *buf, size_t cap, size_t *got)
{
    ng_test_source *source = ctx;
    *got = 0;
    if (source->position >= source->fail_at) {
        return NG_ERR_IO;
    }
    size_t count = source->size - source->position;
    count = count < cap ? count : cap;
    count = count < source->chunk ? count : source->chunk;
    const size_t before_failure = source->fail_at - source->position;
    count = count < before_failure ? count : before_failure;
    if (count != 0u) {
        memcpy(buf, source->data + source->position, count);
    }
    source->position += count;
    *got = count;
    return NG_OK;
}

ng_reader ng_test_chunked_reader(ng_test_source *source, const char *data, size_t size,
                                 size_t chunk)
{
    *source = (ng_test_source){
        .data = data, .size = size, .position = 0, .chunk = chunk, .fail_at = SIZE_MAX};
    return (ng_reader){.read = source_read, .ctx = source};
}

ng_reader ng_test_failing_reader(ng_test_source *source, const char *data, size_t size,
                                 size_t fail_at)
{
    *source = (ng_test_source){
        .data = data, .size = size, .position = 0, .chunk = SIZE_MAX, .fail_at = fail_at};
    return (ng_reader){.read = source_read, .ctx = source};
}

static ng_status sink_write(void *ctx, [[maybe_unused]] const char *buf, size_t len)
{
    ng_test_sink *sink = ctx;
    if (len > sink->fail_at - sink->written) {
        return NG_ERR_IO;
    }
    sink->written += len;
    return NG_OK;
}

ng_writer ng_test_failing_writer(ng_test_sink *sink, size_t fail_at)
{
    *sink = (ng_test_sink){.written = 0, .fail_at = fail_at};
    return (ng_writer){.write = sink_write, .ctx = sink};
}
