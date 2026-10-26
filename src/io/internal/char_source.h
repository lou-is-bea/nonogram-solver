/**
 * @file char_source.h
 * @brief Byte-by-byte view of an ng_reader, read in fixed-size blocks, with the position of
 *        the current byte (ngi, shared by the io readers).
 */
#ifndef NG_IO_INTERNAL_CHAR_SOURCE_H
#define NG_IO_INTERNAL_CHAR_SOURCE_H

#include <stddef.h>
#include <stdint.h>

#include "nonogram/allocator.h"
#include "nonogram/status.h"
#include "nonogram/stream.h"

/** @brief ngi_char_source_peek() value at the end of the input. */
constexpr int NGI_CHAR_END = -1;

/** @brief ngi_char_source_peek() value after a read failure (see ngi_char_source::status). */
constexpr int NGI_CHAR_FAILED = -2;

typedef struct ngi_char_source {
    const ng_reader *reader;
    char *block;
    size_t capacity; /* bytes allocated for block */
    size_t size;     /* valid bytes in block */
    size_t next;     /* index of the current byte in block */
    uint64_t line;   /* 1-based position of the current byte */
    uint64_t column;
    ng_status status; /* NG_OK, or the error of the reader once NGI_CHAR_FAILED is seen */
    bool at_end;
} ngi_char_source;

/**
 * @brief Prepare @p source to read @p reader in blocks of NG_IO_BLOCK_SIZE bytes.
 * @return NG_OK or NG_ERR_OUT_OF_MEMORY. On failure nothing needs releasing.
 */
[[nodiscard]] ng_status ngi_char_source_init(ngi_char_source *source, const ng_allocator *allocator,
                                             const ng_reader *reader);

/** @brief Release the block of a source initialised by ngi_char_source_init(). */
void ngi_char_source_release(ngi_char_source *source, const ng_allocator *allocator);

/**
 * @brief Current byte as an unsigned char value, NGI_CHAR_END or NGI_CHAR_FAILED, without
 *        consuming it.
 */
int ngi_char_source_peek(ngi_char_source *source);

/** @brief Consume the current byte, which must not be NGI_CHAR_END or NGI_CHAR_FAILED. */
void ngi_char_source_advance(ngi_char_source *source);

#endif /* NG_IO_INTERNAL_CHAR_SOURCE_H */
