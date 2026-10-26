/**
 * @file puzzle.h
 * @brief Puzzle: dimensions and clues of a nonogram. Opaque and immutable once created.
 *
 * Line identifiers are unified: 0..height-1 are the rows (top to bottom), height..
 * height+width-1 the columns (left to right). The clue of a row lists its runs left to right,
 * the clue of a column top to bottom. A line without any run has an empty clue.
 *
 * A created puzzle is never modified, so it can be read from several threads at once.
 */
#ifndef NG_NONOGRAM_PUZZLE_H
#define NG_NONOGRAM_PUZZLE_H

#include "nonogram/allocator.h"
#include "nonogram/status.h"
#include "nonogram/types.h"

/** @brief A nonogram definition. Opaque. */
typedef struct ng_puzzle ng_puzzle;

/**
 * @brief Create a puzzle from its clues, given in compressed sparse row (CSR) form.
 *
 * The runs of line L are runs[line_offsets[L]] .. runs[line_offsets[L + 1] - 1], for every
 * line L of the unified numbering. The arrays are copied: the caller may free them as soon as
 * the call returns.
 *
 * Validation, in this order (the first failure is reported):
 * 1. nullptr allocator, line_offsets or out: NG_ERR_INVALID_ARGUMENT;
 * 2. width or height outside [1, NG_DIM_MAX]: NG_ERR_LIMIT;
 * 3. line_offsets[0] != 0 or decreasing offsets: NG_ERR_INVALID_ARGUMENT;
 * 4. a line with more runs than its length allows: NG_ERR_INCONSISTENT (checked on the
 *    offsets alone, before any run is read);
 * 5. runs nullptr while the offsets announce runs, or a run of length 0:
 *    NG_ERR_INVALID_ARGUMENT;
 * 6. a line whose runs plus one gap between consecutive runs exceed its length, or row clues
 *    and column clues with different totals: NG_ERR_INCONSISTENT;
 * 7. a size that is not representable: NG_ERR_LIMIT; allocator failure: NG_ERR_OUT_OF_MEMORY.
 *
 * @param allocator    Allocator for the puzzle. Borrowed: must outlive the puzzle. Must not be
 *                     nullptr.
 * @param width        Number of columns, in [1, NG_DIM_MAX].
 * @param height       Number of rows, in [1, NG_DIM_MAX].
 * @param runs         All run lengths, line after line. May be nullptr only if there is no
 *                     run at all.
 * @param line_offsets height + width + 1 non-decreasing offsets into @p runs, starting at 0.
 *                     Must not be nullptr.
 * @param out          Receives the puzzle on NG_OK, nullptr on failure. Must not be nullptr.
 * @return NG_OK or one of the errors listed above.
 * @ownership The caller owns *out and releases it with ng_puzzle_destroy(). @p runs and
 *            @p line_offsets are only borrowed during the call.
 * @complexity O(height + width + number of runs) time; one allocation of
 *             (height + width + 1) * 8 + (number of runs) * 4 bytes plus a small header.
 */
[[nodiscard]] ng_status ng_puzzle_create(const ng_allocator *allocator, ng_dim width, ng_dim height,
                                         const ng_dim *runs, const ng_run_index *line_offsets,
                                         ng_puzzle **out);

/**
 * @brief Destroy a puzzle.
 *
 * @param puzzle Puzzle to destroy, or nullptr (no-op). Invalid afterwards.
 * @ownership Takes back ownership of @p puzzle.
 * @complexity O(1).
 */
void ng_puzzle_destroy(ng_puzzle *puzzle);

/** @brief Number of columns; 0 for nullptr. O(1). */
ng_dim ng_puzzle_width(const ng_puzzle *puzzle);

/** @brief Number of rows; 0 for nullptr. O(1). */
ng_dim ng_puzzle_height(const ng_puzzle *puzzle);

/** @brief Number of lines, height + width; 0 for nullptr. O(1). */
ng_line_id ng_puzzle_line_count(const ng_puzzle *puzzle);

/** @brief Total number of runs over all lines; 0 for nullptr. O(1). */
ng_run_index ng_puzzle_total_runs(const ng_puzzle *puzzle);

/**
 * @brief Length of a line: the width for a row, the height for a column.
 *
 * This accessor cannot report an error: a nullptr puzzle or a line out of range yields 0, in
 * every build.
 *
 * @complexity O(1).
 */
ng_dim ng_puzzle_line_length(const ng_puzzle *puzzle, ng_line_id line);

/**
 * @brief Number of runs in the clue of a line (0 for an empty line).
 *
 * Same invalid arguments as ng_puzzle_line_length(), yielding 0.
 *
 * @complexity O(1).
 */
ng_run_count ng_puzzle_line_run_count(const ng_puzzle *puzzle, ng_line_id line);

/**
 * @brief Copy the clue of a line into a caller buffer.
 *
 * @param puzzle   Puzzle. Must not be nullptr.
 * @param line     Unified line identifier, below ng_puzzle_line_count().
 * @param buffer   Receives ng_puzzle_line_run_count() run lengths. May be nullptr only when
 *                 the line has no run.
 * @param capacity Number of elements @p buffer can hold.
 * @return NG_OK; NG_ERR_INVALID_ARGUMENT for a nullptr argument, a line out of range or a
 *         capacity below the number of runs (@p buffer is then unchanged).
 * @ownership @p buffer is borrowed.
 * @complexity O(number of runs of the line), no allocation.
 */
[[nodiscard]] ng_status ng_puzzle_copy_line_runs(const ng_puzzle *puzzle, ng_line_id line,
                                                 ng_dim *buffer, ng_run_count capacity);

#endif /* NG_NONOGRAM_PUZZLE_H */
