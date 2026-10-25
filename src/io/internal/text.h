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

#include "core/internal/output.h"

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

#endif /* NG_IO_INTERNAL_TEXT_H */
