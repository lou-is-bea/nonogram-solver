/**
 * @file text.h
 * @brief Text helpers shared by the io readers and writers (ngi): error reports and buffered
 *        output.
 */
#ifndef NG_IO_INTERNAL_TEXT_H
#define NG_IO_INTERNAL_TEXT_H

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include "nonogram/io.h"
#include "nonogram/status.h"
#include "nonogram/stream.h"

/** @brief Room for an escaped token in a message: longer tokens are cut and end with "...". */
constexpr size_t NGI_TOKEN_TEXT_SIZE = 48;

/** @brief Fill @p error (may be nullptr) with a position and a vprintf-style message. */
[[gnu::format(printf, 4, 0)]] void ngi_parse_error_vset(ng_parse_error *error, uint64_t line,
                                                        uint64_t column, const char *format,
                                                        va_list args);

/** @brief Fill @p error (may be nullptr) with a position and a printf-style message. */
[[gnu::format(printf, 4, 5)]] void ngi_parse_error_set(ng_parse_error *error, uint64_t line,
                                                       uint64_t column, const char *format, ...);

/**
 * @brief Write @p bytes into @p out as printable ASCII: tab as \\t, other control or
 *        non-ASCII bytes as \\xHH, cut with "..." when @p out is too small.
 * @param truncated True when @p bytes is itself only the beginning of a longer token.
 */
void ngi_escape(char out[static NGI_TOKEN_TEXT_SIZE], const char *bytes, size_t size,
                bool truncated);

/** @brief Batches small writes into fewer calls to an ng_writer. The first error sticks. */
typedef struct ngi_output {
    const ng_writer *writer;
    ng_status status;
    size_t used;
    char stage[1'024];
} ngi_output;

/** @brief Start an output on @p writer. */
void ngi_output_init(ngi_output *output, const ng_writer *writer);

/** @brief Append @p size bytes. */
void ngi_output_bytes(ngi_output *output, const char *bytes, size_t size);

/** @brief Append a nul-terminated string. */
void ngi_output_text(ngi_output *output, const char *text);

/** @brief Append one byte. */
void ngi_output_char(ngi_output *output, char c);

/** @brief Append the decimal form of @p value. */
void ngi_output_uint(ngi_output *output, uint64_t value);

/** @brief Write what is staged and return the first error met, or NG_OK. */
[[nodiscard]] ng_status ngi_output_finish(ngi_output *output);

#endif /* NG_IO_INTERNAL_TEXT_H */
