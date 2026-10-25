/**
 * @file verify.h
 * @brief Independent verifier: the oracle of every test (ADR-0007, F-20 to F-22).
 *
 * The verifier recomputes the runs of every line of a grid and compares them with the clues.
 * It is deliberately simple and shares no code with any solver.
 */
#ifndef NG_NONOGRAM_VERIFY_H
#define NG_NONOGRAM_VERIFY_H

#include <stdint.h>

#include "nonogram/grid.h"
#include "nonogram/puzzle.h"
#include "nonogram/status.h"
#include "nonogram/types.h"

/** @brief Outcome of a verification. */
enum ng_verdict : uint8_t {
    NG_VERDICT_VALID = 0,  /**< Complete, and every line matches its clue. */
    NG_VERDICT_INCOMPLETE, /**< At least one cell is NG_CELL_UNKNOWN. */
    NG_VERDICT_INVALID,    /**< Complete, but at least one line contradicts its clue. */
};
typedef enum ng_verdict ng_verdict;

/** @brief Details of a verification. Fields not relevant to the verdict are 0. */
typedef struct ng_verify_report {
    ng_verdict verdict;
    /** INVALID: first line, in unified numbering (rows, then columns), whose runs differ from
     *  its clue. */
    ng_line_id first_bad_line;
    /** INVALID: index of the first run of that line that differs from the clue; equal to the
     *  shorter of the two run counts when one list is a prefix of the other. */
    ng_run_count first_bad_run;
    /** Number of NG_CELL_UNKNOWN cells; INCOMPLETE exactly when it is not 0. */
    ng_cell_index unknown_cells;
} ng_verify_report;

/**
 * @brief Check a grid against the clues of a puzzle.
 *
 * Unknown cells take precedence: a grid with any NG_CELL_UNKNOWN cell is INCOMPLETE, whatever
 * its other lines. Otherwise the lines are checked in unified order and the first mismatch is
 * reported.
 *
 * @param puzzle Puzzle. Borrowed, must not be nullptr.
 * @param grid   Grid to check. Borrowed, must not be nullptr, same dimensions as @p puzzle.
 * @param report Receives the verdict. Must not be nullptr. Fully written on NG_OK.
 * @return NG_OK (the verdict is in @p report); NG_ERR_INVALID_ARGUMENT for a nullptr argument
 *         or mismatched dimensions (@p report is then unchanged).
 * @ownership Every argument is borrowed. Nothing is allocated.
 * @complexity O(width x height) time, O(1) extra memory.
 */
[[nodiscard]] ng_status ng_verify(const ng_puzzle *puzzle, const ng_grid *grid,
                                  ng_verify_report *report);

/**
 * @brief Compute the runs actually present on one line of a grid.
 *
 * Unknown cells count as not filled. Used to explain an INVALID verdict next to the expected
 * clue (ng_puzzle_copy_line_runs).
 *
 * @param grid     Grid. Borrowed, must not be nullptr.
 * @param line     Unified line identifier, below height + width.
 * @param buffer   Receives the first min(*count, @p capacity) runs. May be nullptr when
 *                 @p capacity is 0.
 * @param capacity Number of elements @p buffer can hold.
 * @param count    Receives the total number of runs of the line, even when it exceeds
 *                 @p capacity (call once with capacity 0 to size the buffer). Must not be
 *                 nullptr.
 * @return NG_OK; NG_ERR_INVALID_ARGUMENT for a nullptr grid or count, a line out of range, or
 *         a nullptr buffer with a non-zero capacity.
 * @ownership Every argument is borrowed. Nothing is allocated.
 * @complexity O(line length) time, O(1) extra memory.
 */
[[nodiscard]] ng_status ng_verify_line_runs(const ng_grid *grid, ng_line_id line, ng_dim *buffer,
                                            ng_run_count capacity, ng_run_count *count);

#endif /* NG_NONOGRAM_VERIFY_H */
