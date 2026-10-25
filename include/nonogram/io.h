/**
 * @file io.h
 * @brief Reading and writing the text formats .non (puzzle) and .sol (grid).
 *
 * Formats: docs/FILE_FORMATS.md. Every reader works in a stream, in fixed-size blocks, with
 * no limit on file size or line length (ADR-0010).
 */
#ifndef NG_NONOGRAM_IO_H
#define NG_NONOGRAM_IO_H

#include <stddef.h>
#include <stdint.h>

#include "nonogram/allocator.h"
#include "nonogram/grid.h"
#include "nonogram/puzzle.h"
#include "nonogram/status.h"
#include "nonogram/stream.h"

/** @brief Capacity of ng_parse_error::message, terminating nul byte included. */
constexpr size_t NG_PARSE_ERROR_MESSAGE_SIZE = 160;

/** @brief Size of the blocks in which the readers consume their ng_reader: 64 KiB. */
constexpr size_t NG_IO_BLOCK_SIZE = 65'536;

/**
 * @brief Where and why reading failed.
 *
 * Lines and columns are 1-based; the column counts bytes. For an error detected at the end
 * of the input, the position is just past the last byte. On success, line and column are 0
 * and the message is empty.
 */
typedef struct ng_parse_error {
    uint64_t line;
    uint64_t column;
    char message[NG_PARSE_ERROR_MESSAGE_SIZE];
} ng_parse_error;

/** @brief Text directives of a .non file kept by the reader besides the clues. */
enum ng_non_field : uint8_t {
    NG_NON_TITLE = 0,
    NG_NON_AUTHOR,
    NG_NON_COPYRIGHT,
    NG_NON_LICENSE,
    NG_NON_CATALOGUE,
    NG_NON_GOAL,
};
typedef enum ng_non_field ng_non_field;

/** @brief Number of ng_non_field values. */
constexpr size_t NG_NON_FIELD_COUNT = 6;

/** @brief Metadata of a .non file (title, author, ...). Opaque, created by ng_io_read_non(). */
typedef struct ng_non_metadata ng_non_metadata;

/**
 * @brief Return the value of a metadata field, without its quotes.
 *
 * @param metadata Metadata, or nullptr (every field absent).
 * @param field    Field to read.
 * @return The nul-terminated value, or nullptr when the file did not set the field (an empty
 *         string means the file set it to "").
 * @ownership Borrowed from @p metadata; valid until ng_non_metadata_destroy().
 * @complexity O(1).
 */
const char *ng_non_metadata_get(const ng_non_metadata *metadata, ng_non_field field);

/**
 * @brief Destroy metadata returned by ng_io_read_non().
 *
 * @param metadata Metadata to destroy, or nullptr (no-op).
 * @ownership Takes back ownership of @p metadata.
 * @complexity O(1).
 */
void ng_non_metadata_destroy(ng_non_metadata *metadata);

/**
 * @brief Read a puzzle in .non format.
 *
 * The reader consumes @p reader in blocks of NG_IO_BLOCK_SIZE bytes; besides the puzzle under
 * construction, it uses O(1) memory.
 *
 * @param allocator Allocator for the puzzle, the metadata and the reading state. Borrowed:
 *                  must outlive the returned objects. Must not be nullptr.
 * @param reader    Source. Borrowed. Must not be nullptr.
 * @param out       Receives the puzzle on NG_OK, nullptr on failure. Must not be nullptr.
 * @param metadata  Receives the metadata on NG_OK (never nullptr then), nullptr on failure.
 *                  May be nullptr: the directives are then validated and discarded.
 * @param error     Receives the position and description of a failure. May be nullptr.
 * @return NG_OK;
 *         NG_ERR_INVALID_ARGUMENT for a nullptr argument;
 *         NG_ERR_PARSE for malformed input;
 *         NG_ERR_INCONSISTENT when a clue cannot fit its line or row and column totals
 *         differ;
 *         NG_ERR_LIMIT when the puzzle is not representable;
 *         NG_ERR_IO when @p reader fails;
 *         NG_ERR_OUT_OF_MEMORY when the allocator fails.
 * @ownership The caller owns *out (ng_puzzle_destroy) and *metadata
 *            (ng_non_metadata_destroy).
 * @complexity O(file size) time. Memory: the final puzzle (8 bytes per line and 4 per run),
 *             plus, while it is built, the runs read so far in a buffer that doubles when full
 *             (up to twice the clue storage). Peak: about 3 times the clue storage, plus the
 *             64 KiB read block.
 */
[[nodiscard]] ng_status ng_io_read_non(const ng_allocator *allocator, const ng_reader *reader,
                                       ng_puzzle **out, ng_non_metadata **metadata,
                                       ng_parse_error *error);

/**
 * @brief Write a puzzle in .non format.
 *
 * The output reads back with ng_io_read_non() to the same clues and metadata. Clues are
 * written with commas ("1,1") and "0" for an empty line.
 *
 * @param puzzle   Puzzle. Must not be nullptr.
 * @param metadata Directives to write before the clues (the goal after them). May be nullptr.
 * @param writer   Destination. Must not be nullptr.
 * @return NG_OK; NG_ERR_INVALID_ARGUMENT for a nullptr argument; otherwise the first error
 *         of @p writer.
 * @ownership Every argument is borrowed. Nothing is allocated.
 * @complexity O(lines + runs) time, O(1) memory.
 */
[[nodiscard]] ng_status ng_io_write_non(const ng_puzzle *puzzle, const ng_non_metadata *metadata,
                                        const ng_writer *writer);

/**
 * @brief Read a grid in .sol format.
 *
 * The dimensions come from the content: the width from the first line, the height from the
 * number of lines. The cells are accumulated in a buffer that doubles when full, then copied
 * into the grid, so the peak memory reaches about 3 times the grid (buffer of up to twice the
 * cells, plus the grid), plus the 64 KiB read block.
 *
 * @param allocator Allocator for the grid and the reading state. Borrowed: must outlive the
 *                  grid. Must not be nullptr.
 * @param reader    Source. Borrowed. Must not be nullptr.
 * @param out       Receives the grid on NG_OK, nullptr on failure. Must not be nullptr.
 * @param error     Receives the position and description of a failure. May be nullptr.
 * @return NG_OK; NG_ERR_INVALID_ARGUMENT for a nullptr argument; NG_ERR_PARSE for malformed
 *         input (empty file, unknown character, lines of different lengths);
 *         NG_ERR_LIMIT for a dimension above NG_DIM_MAX; NG_ERR_IO when @p reader fails;
 *         NG_ERR_OUT_OF_MEMORY when the allocator fails.
 * @ownership The caller owns *out (ng_grid_destroy).
 * @complexity O(file size) time; up to about 3 x width x height bytes at peak.
 */
[[nodiscard]] ng_status ng_io_read_sol(const ng_allocator *allocator, const ng_reader *reader,
                                       ng_grid **out, ng_parse_error *error);

/**
 * @brief Write a grid in .sol format: one line per row, '#' filled, '.' empty, '?' unknown,
 *        each line ending with '\n'.
 *
 * @param grid   Grid. Must not be nullptr.
 * @param writer Destination. Must not be nullptr.
 * @return NG_OK; NG_ERR_INVALID_ARGUMENT for a nullptr argument; otherwise the first error
 *         of @p writer.
 * @ownership Every argument is borrowed. Nothing is allocated.
 * @complexity O(width x height) time, O(1) memory.
 */
[[nodiscard]] ng_status ng_io_write_sol(const ng_grid *grid, const ng_writer *writer);

#endif /* NG_NONOGRAM_IO_H */
