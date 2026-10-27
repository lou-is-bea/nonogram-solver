#include "core/internal/output.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "nonogram/status.h"
#include "nonogram/stream.h"

static constexpr uint64_t decimal_base = 10;
static constexpr size_t uint64_max_digits = 20;

void ngi_output_init(ngi_output *output, const ng_writer *writer)
{
    output->writer = writer;
    output->status = NG_OK;
    output->used = 0;
}

static void flush(ngi_output *output)
{
    if (output->status == NG_OK && output->used != 0u) {
        output->status = output->writer->write(output->writer->ctx, output->stage, output->used);
    }
    output->used = 0;
}

void ngi_output_bytes(ngi_output *output, const char *bytes, size_t size)
{
    while (size != 0u && output->status == NG_OK) {
        if (output->used == sizeof output->stage) {
            flush(output);
        }
        const size_t room = sizeof output->stage - output->used;
        const size_t count = size < room ? size : room;
        memcpy(output->stage + output->used, bytes, count);
        output->used += count;
        bytes += count;
        size -= count;
    }
}

void ngi_output_text(ngi_output *output, const char *text)
{
    ngi_output_bytes(output, text, strlen(text));
}

void ngi_output_char(ngi_output *output, char c)
{
    ngi_output_bytes(output, &c, 1);
}

void ngi_output_uint(ngi_output *output, uint64_t value)
{
    char digits[uint64_max_digits] = {};
    size_t count = 0;
    do {
        digits[sizeof digits - 1u - count] = (char)('0' + (value % decimal_base));
        value /= decimal_base;
        ++count;
    } while (value != 0u);
    ngi_output_bytes(output, digits + sizeof digits - count, count);
}

[[nodiscard]] ng_status ngi_output_finish(ngi_output *output)
{
    flush(output);
    return output->status;
}
