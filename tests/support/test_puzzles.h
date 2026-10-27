/**
 * @file test_puzzles.h
 * @brief Random grids and the puzzles whose clues they satisfy, for property tests.
 */
#ifndef NG_TESTS_SUPPORT_TEST_PUZZLES_H
#define NG_TESTS_SUPPORT_TEST_PUZZLES_H

#include "nonogram/grid.h"
#include "nonogram/puzzle.h"
#include "nonogram/types.h"

#include "test_rng.h"

/**
 * @brief Random grid of 1..max_side x 1..max_side cells, allocated on ng_test_heap(), with a
 *        random density. Cells are FILLED or EMPTY, plus UNKNOWN when @p with_unknown.
 */
ng_grid *ng_test_random_grid(ng_test_rng *rng, ng_dim max_side, bool with_unknown);

/**
 * @brief Grid drawn as @p height rows of '#' (filled), '.' (empty) and '?' (unknown), all of
 *        the same length, allocated on ng_test_heap().
 */
ng_grid *ng_test_grid_of(const char *const *rows, ng_dim height);

/**
 * @brief Puzzle whose clues are the runs of @p grid (UNKNOWN counts as EMPTY), allocated on
 *        ng_test_heap(). Fails the test if creation fails.
 */
ng_puzzle *ng_test_puzzle_from_grid(const ng_grid *grid);

/** @brief Transposed copy of @p grid (rows become columns), allocated on ng_test_heap(). */
ng_grid *ng_test_transpose_grid(const ng_grid *grid);

/**
 * @brief Transposed copy of @p puzzle, built by swapping its row and column clue blocks
 *        (without going through any grid), allocated on ng_test_heap().
 */
ng_puzzle *ng_test_transpose_puzzle(const ng_puzzle *puzzle);

/** @brief Fails the test unless @p a and @p b have the same dimensions and clues. */
void ng_test_assert_same_clues(const ng_puzzle *a, const ng_puzzle *b);

#endif /* NG_TESTS_SUPPORT_TEST_PUZZLES_H */
