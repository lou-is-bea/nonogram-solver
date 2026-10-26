#include "io/internal/text.h"

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "nonogram/io.h"
#include "nonogram/status.h"
#include "nonogram/stream.h"

void ngi_parse_error_vset(ng_parse_error *error, uint64_t line, uint64_t column, const char *format,
                          va_list args)
{
    if (error == nullptr) {
        return;
    }
    error->line = line;
    error->column = column;
    /* A message cut to the buffer size is acceptable: it only serves diagnostics. */
    if (vsnprintf(error->message, sizeof error->message, format, args) < 0) {
        error->message[0] = '\0';
    }
}

void ngi_parse_error_set(ng_parse_error *error, uint64_t line, uint64_t column, const char *format,
                         ...)
{
    va_list args;
    va_start(args, format);
    ngi_parse_error_vset(error, line, column, format, args);
    va_end(args);
}

static constexpr char hex_digits[] = "0123456789ABCDEF";
static constexpr unsigned nibble_bits = 4;
static constexpr unsigned nibble_mask = 0x0F;
static constexpr unsigned char first_printable = 0x20; /* space */
static constexpr unsigned char delete_char = 0x7F;
static constexpr uint64_t decimal_base = 10;
static constexpr size_t uint64_max_digits = 20;
static constexpr size_t ellipsis_size = 3; /* "..." */
static constexpr size_t widest_escape = 4; /* "\xHH" */

void ngi_escape(char out[static NGI_TOKEN_TEXT_SIZE], const char *bytes, size_t size,
                bool truncated)
{
    const size_t limit = NGI_TOKEN_TEXT_SIZE - ellipsis_size - 1u;
    size_t used = 0;
    size_t i = 0;
    for (; i < size && used + widest_escape <= limit; ++i) {
        const unsigned char c = (unsigned char)bytes[i];
        if (c == '\t') {
            out[used++] = '\\';
            out[used++] = 't';
        } else if (c >= first_printable && c < delete_char) {
            out[used++] = (char)c;
        } else {
            out[used++] = '\\';
            out[used++] = 'x';
            out[used++] = hex_digits[c >> nibble_bits];
            out[used++] = hex_digits[c & nibble_mask];
        }
    }
    if (i < size || truncated) {
        memcpy(out + used, "...", ellipsis_size);
        used += ellipsis_size;
    }
    out[used] = '\0';
}

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
