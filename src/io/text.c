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
