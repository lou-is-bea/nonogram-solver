#include <criterion/criterion.h>
#include <inttypes.h>
#include <stdint.h>
#include <string.h>

#include "nonogram/grid.h"
#include "nonogram/puzzle.h"
#include "nonogram/solver.h"
#include "nonogram/status.h"
#include "nonogram/types.h"
#include "nonogram/verify.h"

#include "ng_test_fixture.h"
#include "test_puzzles.h"
#include "test_rng.h"

/* P-S1, P-S4 and P-S5 for S0 (TESTING.md §4.4, docs/solvers/s0_brute.md §6). G is a random
 * complete grid of at most 10 cells and P = clues(G), so that 1 000 cases stay within 2^20
 * candidates per test; P-S5 solves twice, on at most 9 cells. NG_PROPERTY_CASES=100000 gives
 * the nightly volume (G5: 0.75 s in the dev preset, measured 2026-10-29), well within the
 * timeout. P-S6 is a unit test. */
TestSuite(prop_solver_brute, .init = ng_test_setup, .fini = ng_test_teardown, .timeout = 60);

static constexpr uint64_t default_cases = 1'000;
static constexpr ng_cell_index max_cells = 10;
static constexpr ng_cell_index max_cells_twice = 9;

/* Random complete grid of 1 to @p cells_limit cells, any shape (1 x n, n x 1, ...). */
static ng_grid *random_complete_grid(ng_test_rng *rng, ng_cell_index cells_limit)
{
    const ng_dim width = (ng_dim)(1u + ng_test_rng_below(rng, cells_limit));
    const ng_dim height = (ng_dim)(1u + ng_test_rng_below(rng, cells_limit / width));
    ng_grid *grid = nullptr;
    cr_assert_eq(ng_grid_create(&ng_test_heap()->base, width, height, &grid), NG_OK);
    for (ng_dim r = 0; r < height; ++r) {
        for (ng_dim c = 0; c < width; ++c) {
            ng_grid_set(grid, r, c, ng_test_rng_below(rng, 2) ? NG_CELL_FILLED : NG_CELL_EMPTY);
        }
    }
    return grid;
}

static ng_grid *unknown_grid_like(const ng_grid *grid)
{
    ng_grid *out = nullptr;
    cr_assert_eq(
        ng_grid_create(&ng_test_heap()->base, ng_grid_width(grid), ng_grid_height(grid), &out),
        NG_OK);
    return out;
}

static ng_solve_outcome solve(const ng_puzzle *puzzle, ng_grid *grid)
{
    ng_solve_outcome outcome = {};
    cr_assert_eq(
        ng_solve(&ng_test_heap()->base, ng_solver_find("brute"), puzzle, nullptr, grid, &outcome),
        NG_OK);
    return outcome;
}

static ng_verdict verdict(const ng_puzzle *puzzle, const ng_grid *grid)
{
    ng_verify_report report = {};
    cr_assert_eq(ng_verify(puzzle, grid, &report), NG_OK);
    return report.verdict;
}

static bool same_cells(const ng_grid *a, const ng_grid *b)
{
    for (ng_dim r = 0; r < ng_grid_height(a); ++r) {
        for (ng_dim c = 0; c < ng_grid_width(a); ++c) {
            if (ng_grid_get(a, r, c) != ng_grid_get(b, r, c)) {
                return false;
            }
        }
    }
    return true;
}

Test(prop_solver_brute, p_s1_a_complete_solver_solves_the_clues_of_any_grid)
{
    const uint64_t seed = ng_test_seed();
    const uint64_t cases = ng_test_case_count(default_cases);
    ng_test_rng rng;
    ng_test_rng_seed(&rng, seed);

    for (uint64_t k = 0; k < cases; ++k) {
        ng_grid *drawing = random_complete_grid(&rng, max_cells);
        ng_puzzle *puzzle = ng_test_puzzle_from_grid(drawing);
        ng_grid *grid = unknown_grid_like(drawing);

        const ng_solve_outcome outcome = solve(puzzle, grid);

        cr_assert_eq(outcome.result, NG_SOLVE_SOLVED, "seed %" PRIu64 ", case %" PRIu64, seed, k);
        cr_assert_eq(verdict(puzzle, grid), NG_VERDICT_VALID, "seed %" PRIu64 ", case %" PRIu64,
                     seed, k);
        ng_grid_destroy(grid);
        ng_puzzle_destroy(puzzle);
        ng_grid_destroy(drawing);
    }
}

/* Copies a random subset of the cells of @p drawing into @p grid; flips one of them when
 * @p contradict is set and at least one cell was copied. */
static void reveal_cells(ng_test_rng *rng, const ng_grid *drawing, ng_grid *grid, bool contradict)
{
    bool flipped = !contradict;
    for (ng_dim r = 0; r < ng_grid_height(drawing); ++r) {
        for (ng_dim c = 0; c < ng_grid_width(drawing); ++c) {
            if (ng_test_rng_below(rng, 2) == 0u) {
                continue;
            }
            ng_cell cell = ng_grid_get(drawing, r, c);
            if (!flipped) {
                cell = cell == NG_CELL_FILLED ? NG_CELL_EMPTY : NG_CELL_FILLED;
                flipped = true;
            }
            ng_grid_set(grid, r, c, cell);
        }
    }
}

/* P-S4: every cell known on input keeps its value, whatever the result. */
static void assert_known_cells_kept(const ng_grid *before, const ng_grid *after, uint64_t seed,
                                    uint64_t k)
{
    for (ng_dim r = 0; r < ng_grid_height(before); ++r) {
        for (ng_dim c = 0; c < ng_grid_width(before); ++c) {
            const ng_cell known = ng_grid_get(before, r, c);
            if (known != NG_CELL_UNKNOWN) {
                cr_assert_eq(ng_grid_get(after, r, c), known,
                             "seed %" PRIu64 ", case %" PRIu64 ", cell (%u, %u)", seed, k, r, c);
            }
        }
    }
}

Test(prop_solver_brute, p_s4_known_cells_are_never_modified)
{
    const uint64_t seed = ng_test_seed();
    const uint64_t cases = ng_test_case_count(default_cases);
    ng_test_rng rng;
    ng_test_rng_seed(&rng, seed);

    for (uint64_t k = 0; k < cases; ++k) {
        ng_grid *drawing = random_complete_grid(&rng, max_cells);
        ng_puzzle *puzzle = ng_test_puzzle_from_grid(drawing);
        ng_grid *before = unknown_grid_like(drawing);
        const bool contradict = (k % 2u) == 1u;
        reveal_cells(&rng, drawing, before, contradict);
        ng_grid *grid = unknown_grid_like(drawing);
        cr_assert_eq(ng_grid_copy(grid, before), NG_OK);

        const ng_solve_outcome outcome = solve(puzzle, grid);

        assert_known_cells_kept(before, grid, seed, k);
        if (outcome.result == NG_SOLVE_SOLVED) {
            cr_assert_eq(verdict(puzzle, grid), NG_VERDICT_VALID);
        } else {
            /* Only a contradiction can make the clues of a grid unsolvable. */
            cr_assert(contradict, "seed %" PRIu64 ", case %" PRIu64, seed, k);
            cr_assert_eq(outcome.result, NG_SOLVE_UNSOLVABLE);
            cr_assert(same_cells(before, grid), "an unsolvable grid is left unchanged");
        }
        ng_grid_destroy(grid);
        ng_grid_destroy(before);
        ng_puzzle_destroy(puzzle);
        ng_grid_destroy(drawing);
    }
}

Test(prop_solver_brute, p_s5_two_runs_give_the_same_result_statistics_and_grid)
{
    const uint64_t seed = ng_test_seed();
    const uint64_t cases = ng_test_case_count(default_cases);
    ng_test_rng rng;
    ng_test_rng_seed(&rng, seed);

    for (uint64_t k = 0; k < cases; ++k) {
        ng_grid *drawing = random_complete_grid(&rng, max_cells_twice);
        ng_puzzle *puzzle = ng_test_puzzle_from_grid(drawing);
        ng_grid *first = unknown_grid_like(drawing);
        ng_grid *second = unknown_grid_like(drawing);

        const ng_solve_outcome a = solve(puzzle, first);
        const ng_solve_outcome b = solve(puzzle, second);

        cr_assert_eq(a.result, b.result, "seed %" PRIu64 ", case %" PRIu64, seed, k);
        cr_assert_eq(a.stats.nodes, b.stats.nodes, "seed %" PRIu64 ", case %" PRIu64, seed, k);
        cr_assert_eq(a.stats.solutions_found, b.stats.solutions_found);
        cr_assert(same_cells(first, second), "seed %" PRIu64 ", case %" PRIu64, seed, k);
        ng_grid_destroy(second);
        ng_grid_destroy(first);
        ng_puzzle_destroy(puzzle);
        ng_grid_destroy(drawing);
    }
}
