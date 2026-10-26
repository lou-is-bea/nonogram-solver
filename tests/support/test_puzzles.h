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
 * @brief Puzzle whose clues are the runs of @p grid (UNKNOWN counts as EMPTY), allocated on
 *        ng_test_heap(). Fails the test if creation fails.
 */
ng_puzzle *ng_test_puzzle_from_grid(const ng_grid *grid);

/** @brief Fails the test unless @p a and @p b have the same dimensions and clues. */
void ng_test_assert_same_clues(const ng_puzzle *a, const ng_puzzle *b);

#endif /* NG_TESTS_SUPPORT_TEST_PUZZLES_H */
