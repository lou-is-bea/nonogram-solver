/**
 * @file stream.h
 * @brief Byte streams: ng_reader (input) and ng_writer (output), over files or memory.
 *
 * Every parser reads through an ng_reader and every formatter writes through an ng_writer,
 * in fixed-size blocks, so that no file is ever loaded whole in memory (ADR-0010).
 */
#ifndef NG_NONOGRAM_STREAM_H
#define NG_NONOGRAM_STREAM_H

#include <stddef.h>
#include <stdio.h>

#include "nonogram/allocator.h"
#include "nonogram/status.h"

/**
 * @brief Input stream: a read function and its context.
 *
 * Contract of read(ctx, buf, cap, got):
 * - reads at most @p cap bytes into @p buf and stores the number read in @p got;
 * - may return fewer than @p cap bytes before the end of the stream (short read);
 * - NG_OK with *got == 0 means end of stream, and keeps meaning it on later calls;
 * - NG_ERR_INVALID_ARGUMENT if @p buf or @p got is nullptr, if @p cap is 0, or if the
 *   context is invalid;
 * - NG_ERR_IO on a read failure; *got is then 0.
 */
typedef struct ng_reader {
    ng_status (*read)(void *ctx, char *buf, size_t cap, size_t *got);
    void *ctx;
} ng_reader;

/**
 * @brief Output stream: a write function and its context.
 *
 * Contract of write(ctx, buf, len):
 * - writes all @p len bytes of @p buf, or reports an error;
 * - a zero @p len is a no-op returning NG_OK (and @p buf may then be nullptr);
 * - NG_ERR_INVALID_ARGUMENT if @p buf is nullptr with a non-zero @p len, or if the context is
 *   invalid;
 * - other errors depend on the destination (NG_ERR_IO, NG_ERR_OUT_OF_MEMORY, NG_ERR_LIMIT).
 */
typedef struct ng_writer {
    ng_status (*write)(void *ctx, const char *buf, size_t len);
    void *ctx;
} ng_writer;

/**
 * @brief State of a reader over a memory block, provided by the caller.
 *
 * Fields are initialised by ng_reader_from_memory() and must not be modified while the
 * reader is in use.
 */
typedef struct ng_memory_source {
    const char *data; /**< Start of the block. */
    size_t size;      /**< Size of the block in bytes. */
    size_t position;  /**< Bytes already read. */
} ng_memory_source;

/** @brief Growable memory buffer, the destination of ng_writer_from_buffer(). Opaque. */
typedef struct ng_buffer ng_buffer;

/**
 * @brief Make a reader over an open stdio stream.
 *
 * @param file Stream opened for reading. Borrowed: the caller keeps it open while the reader
 *             is used, and closes it. nullptr yields a reader whose read() returns
 *             NG_ERR_INVALID_ARGUMENT.
 * @return The reader, by value.
 * @ownership The reader borrows @p file.
 * @complexity O(1). Each read() is O(cap).
 */
ng_reader ng_reader_from_file(FILE *file);

/**
 * @brief Make a reader over a memory block, using caller-provided state.
 *
 * @param source State to initialise. Borrowed: must outlive the reader. nullptr yields a
 *               reader whose read() returns NG_ERR_INVALID_ARGUMENT.
 * @param data   Block to read. Borrowed: must outlive the reader. May be nullptr only if
 *               @p size is 0; otherwise read() returns NG_ERR_INVALID_ARGUMENT.
 * @param size   Size of the block in bytes.
 * @return The reader, by value.
 * @ownership The reader borrows @p source and @p data; nothing is allocated.
 * @complexity O(1). Each read() is O(cap).
 */
ng_reader ng_reader_from_memory(ng_memory_source *source, const char *data, size_t size);

/**
 * @brief Make a writer over an open stdio stream.
 *
 * Errors of buffered writes may only surface when the caller flushes or closes @p file: the
 * caller checks fflush() or fclose() as well.
 *
 * @param file Stream opened for writing. Borrowed: the caller keeps it open while the writer
 *             is used, and closes it. nullptr yields a writer whose write() returns
 *             NG_ERR_INVALID_ARGUMENT.
 * @return The writer, by value. Its write() returns NG_ERR_IO on a short write.
 * @ownership The writer borrows @p file.
 * @complexity O(1). Each write() is O(len).
 */
ng_writer ng_writer_from_file(FILE *file);

/**
 * @brief Make a writer that appends to a growable buffer.
 *
 * A write is all-or-nothing: on failure the buffer content is unchanged.
 *
 * @param buffer Destination. Borrowed: must outlive the writer. nullptr yields a writer whose
 *               write() returns NG_ERR_INVALID_ARGUMENT.
 * @return The writer, by value. Its write() returns NG_ERR_OUT_OF_MEMORY when the buffer
 *         allocator refuses to grow, NG_ERR_LIMIT when the new size is not representable.
 * @ownership The writer borrows @p buffer.
 * @complexity O(1). Each write() is amortised O(len): the capacity doubles when exceeded.
 */
ng_writer ng_writer_from_buffer(ng_buffer *buffer);

/**
 * @brief Create an empty growable buffer.
 *
 * The content storage is allocated on the first non-empty write, through @p allocator.
 *
 * @param allocator Allocator for the buffer and its content. Borrowed: must outlive the
 *                  buffer. Must not be nullptr.
 * @param out       Receives the buffer on NG_OK, nullptr on failure. Must not be nullptr.
 * @return NG_OK, NG_ERR_INVALID_ARGUMENT for a nullptr argument, NG_ERR_OUT_OF_MEMORY when
 *         the allocator fails.
 * @ownership The caller owns *out and releases it with ng_buffer_destroy().
 * @complexity O(1) time; one allocation of a small fixed size.
 */
[[nodiscard]] ng_status ng_buffer_create(const ng_allocator *allocator, ng_buffer **out);

/**
 * @brief Destroy a buffer and its content.
 *
 * @param buffer Buffer to destroy, or nullptr (no-op). Invalid afterwards.
 * @ownership Takes back ownership of @p buffer.
 * @complexity O(1).
 */
void ng_buffer_destroy(ng_buffer *buffer);

/**
 * @brief Return the buffer content.
 *
 * The content is always followed by a nul byte, so text can be used as a C string; binary
 * content may itself contain nul bytes, so use ng_buffer_size() for its length.
 *
 * @param buffer Buffer, or nullptr (treated as empty).
 * @return The content; an empty string when the buffer is empty. Never nullptr.
 * @ownership Borrowed from the buffer; invalidated by the next write or by destruction.
 * @complexity O(1).
 */
const char *ng_buffer_data(const ng_buffer *buffer);

/**
 * @brief Return the number of bytes written to the buffer.
 *
 * @param buffer Buffer, or nullptr (treated as empty).
 * @return The content size in bytes, terminating nul byte excluded.
 * @complexity O(1).
 */
size_t ng_buffer_size(const ng_buffer *buffer);

#endif /* NG_NONOGRAM_STREAM_H */
