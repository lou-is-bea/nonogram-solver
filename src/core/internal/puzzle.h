/**
 * @file puzzle.h
 * @brief Read-only fast access to the clues of a puzzle (ngi), for the io and render layers.
 *
 * Solvers use the public line copies instead (ARCHITECTURE.md §3).
 */
#ifndef NG_CORE_INTERNAL_PUZZLE_H
#define NG_CORE_INTERNAL_PUZZLE_H

#include "nonogram/puzzle.h"
#include "nonogram/types.h"

/**
 * @brief Runs of @p line, ng_puzzle_line_run_count() of them, without copying.
 * @return A pointer into the puzzle, valid while it lives; nullptr for a line without runs.
 *         @p puzzle must not be nullptr and @p line must be in range (asserted).
 */
const ng_dim *ngi_puzzle_line_runs(const ng_puzzle *puzzle, ng_line_id line);

#endif /* NG_CORE_INTERNAL_PUZZLE_H */
