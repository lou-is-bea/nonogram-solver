/**
 * @file output.h
 * @brief Buffered text output over an ng_writer (ngi), shared by the io and render layers.
 */
#ifndef NG_CORE_INTERNAL_OUTPUT_H
#define NG_CORE_INTERNAL_OUTPUT_H

#include <stddef.h>
#include <stdint.h>

#include "nonogram/status.h"
#include "nonogram/stream.h"

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

#endif /* NG_CORE_INTERNAL_OUTPUT_H */
