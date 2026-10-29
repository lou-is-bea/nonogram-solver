#include <criterion/criterion.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nonogram/grid.h"
#include "nonogram/io.h"
#include "nonogram/puzzle.h"
#include "nonogram/solver.h"
#include "nonogram/status.h"
#include "nonogram/stream.h"
#include "nonogram/types.h"
#include "nonogram/verify.h"

#include "corpus_files.h"
#include "ng_test_fixture.h"

/* Exit criterion of M4 (docs/ROADMAP.md, docs/solvers/s0_brute.md §6): S0 solves the whole
 * corpus within its 25-cell limit, from an empty grid, and the verifier validates every
 * solution. Up to 2^25 candidates per puzzle: label "exhaustive", outside Valgrind (G7). */
TestSuite(exhaustive_brute, .init = ng_test_setup, .fini = ng_test_teardown, .timeout = 120);

static constexpr ng_cell_index brute_max_cells = 25;

static ng_puzzle *read_puzzle(const char *path)
{
    FILE *file = fopen(path, "rb");
    cr_assert_not_null(file, "cannot open %s", path);
    const ng_reader r = ng_reader_from_file(file);
    ng_puzzle *puzzle = nullptr;
    ng_parse_error error = {};
    cr_assert_eq(ng_io_read_non(&ng_test_heap()->base, &r, &puzzle, nullptr, &error), NG_OK,
                 "%s: %s", path, error.message);
    cr_assert_eq(fclose(file), 0);
    return puzzle;
}

static ng_grid *read_solution(const char *path)
{
    FILE *file = fopen(path, "rb");
    cr_assert_not_null(file, "cannot open %s", path);
    const ng_reader r = ng_reader_from_file(file);
    ng_grid *grid = nullptr;
    ng_parse_error error = {};
    cr_assert_eq(ng_io_read_sol(&ng_test_heap()->base, &r, &grid, &error), NG_OK, "%s: %s", path,
                 error.message);
    cr_assert_eq(fclose(file), 0);
    return grid;
}

static ng_cell_index cells_of(const ng_puzzle *puzzle)
{
    return (ng_cell_index)ng_puzzle_width(puzzle) * ng_puzzle_height(puzzle);
}

/* Solves @p puzzle from an empty grid, counting up to @p max_solutions; @p grid receives the
 * first solution. */
static ng_solve_outcome count_solutions(const ng_puzzle *puzzle, uint32_t max_solutions,
                                        ng_grid **grid)
{
    cr_assert_eq(ng_grid_create(&ng_test_heap()->base, ng_puzzle_width(puzzle),
                                ng_puzzle_height(puzzle), grid),
                 NG_OK);
    ng_solve_options options = ng_solve_options_default();
    options.max_solutions = max_solutions;
    ng_solve_outcome outcome = {};
    cr_assert_eq(
        ng_solve(&ng_test_heap()->base, ng_solver_find("brute"), puzzle, &options, *grid, &outcome),
        NG_OK);
    return outcome;
}

static void assert_valid(const ng_puzzle *puzzle, const ng_grid *grid, const char *path)
{
    ng_verify_report report = {};
    cr_assert_eq(ng_verify(puzzle, grid, &report), NG_OK);
    cr_assert_eq(report.verdict, NG_VERDICT_VALID, "%s", path);
}

/* Every tiny puzzle: solved, unique (re-verifying tests/data/README.md), equal to its .sol. */
Test(exhaustive_brute, every_tiny_puzzle_is_solved_uniquely_and_validated)
{
    size_t tiny_count = 0;
    for (size_t i = 0; i < sizeof ng_corpus_solved / sizeof ng_corpus_solved[0]; ++i) {
        ng_puzzle *puzzle = read_puzzle(ng_corpus_solved[i].puzzle);
        if (cells_of(puzzle) > brute_max_cells) { /* small/: beyond S0 */
            ng_puzzle_destroy(puzzle);
            continue;
        }
        cr_assert_not_null(strstr(ng_corpus_solved[i].puzzle, "/puzzles/tiny/"), "%s",
                           ng_corpus_solved[i].puzzle);
        ++tiny_count;
        ng_grid *grid = nullptr;

        const ng_solve_outcome outcome = count_solutions(puzzle, 2, &grid);

        cr_assert_eq(outcome.result, NG_SOLVE_SOLVED, "%s", ng_corpus_solved[i].puzzle);
        cr_assert_eq(outcome.stats.solutions_found, 1u, "%s is unique", ng_corpus_solved[i].puzzle);
        assert_valid(puzzle, grid, ng_corpus_solved[i].puzzle);
        ng_grid *expected = read_solution(ng_corpus_solved[i].solution);
        for (ng_dim r = 0; r < ng_grid_height(grid); ++r) {
            for (ng_dim c = 0; c < ng_grid_width(grid); ++c) {
                cr_assert_eq(ng_grid_get(grid, r, c), ng_grid_get(expected, r, c), "%s (%u, %u)",
                             ng_corpus_solved[i].puzzle, r, c);
            }
        }
        ng_grid_destroy(expected);
        ng_grid_destroy(grid);
        ng_puzzle_destroy(puzzle);
    }
    cr_assert_gt(tiny_count, 0u, "the tiny corpus is empty");
}

/* Expected number of solutions of puzzles/multi/<name>_n<count>.non. */
static uint32_t expected_count(const char *path)
{
    const char *marker = strrchr(path, '_');
    cr_assert(marker != nullptr && marker[1] == 'n', "%s: no _n<count> suffix", path);
    char *end = nullptr;
    const unsigned long count = strtoul(marker + 2, &end, 10);
    cr_assert(end != nullptr && strcmp(end, ".non") == 0 && count > 1u && count < UINT32_MAX, "%s",
              path);
    return (uint32_t)count;
}

/* Multi puzzles within the limit (exit criterion, decision 4 of the specification): exactly
 * the count of their name. The unsolvable ones: no solution. */
Test(exhaustive_brute, multi_and_unsolvable_puzzles_within_the_limit_have_their_count)
{
    size_t multi_count = 0;
    size_t unsolvable_count = 0;
    for (size_t i = 0; i < sizeof ng_corpus_other / sizeof ng_corpus_other[0]; ++i) {
        const char *path = ng_corpus_other[i];
        const bool multi = strstr(path, "/puzzles/multi/") != nullptr;
        const bool unsolvable = strstr(path, "/puzzles/unsolvable/") != nullptr;
        if (!multi && !unsolvable) {
            continue;
        }
        ng_puzzle *puzzle = read_puzzle(path);
        if (cells_of(puzzle) > brute_max_cells) {
            ng_puzzle_destroy(puzzle);
            continue;
        }
        const uint32_t expected = multi ? expected_count(path) : 0u;
        ng_grid *grid = nullptr;

        const ng_solve_outcome outcome = count_solutions(puzzle, expected + 1u, &grid);

        cr_assert_eq(outcome.stats.solutions_found, expected, "%s", path);
        if (multi) {
            ++multi_count;
            cr_assert_eq(outcome.result, NG_SOLVE_SOLVED, "%s", path);
            assert_valid(puzzle, grid, path);
        } else {
            ++unsolvable_count;
            cr_assert_eq(outcome.result, NG_SOLVE_UNSOLVABLE, "%s", path);
        }
        ng_grid_destroy(grid);
        ng_puzzle_destroy(puzzle);
    }
    cr_assert_geq(multi_count, 2u, "multi puzzles within the limit");
    cr_assert_geq(unsolvable_count, 1u);
}
