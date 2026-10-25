#include <criterion/criterion.h>
#include <inttypes.h>
#include <stdint.h>

#include "nonogram/grid.h"
#include "nonogram/puzzle.h"
#include "nonogram/status.h"
#include "nonogram/types.h"
#include "nonogram/verify.h"

#include "ng_test_fixture.h"
#include "test_puzzles.h"
#include "test_rng.h"

/* P-V1 to P-V4 (TESTING.md §4.4): G is a random complete grid, P = clues(G). 1 000 cases per
 * property by default; NG_PROPERTY_CASES=100000 for the nightly volume. */
TestSuite(prop_verify, .init = ng_test_setup, .fini = ng_test_teardown, .timeout = 3'600);

static constexpr uint64_t default_cases = 1'000;
static constexpr ng_dim max_side = 30;

static ng_verify_report verify(const ng_puzzle *puzzle, const ng_grid *grid)
{
    ng_verify_report report = {};
    cr_assert_eq(ng_verify(puzzle, grid, &report), NG_OK);
    return report;
}

typedef struct random_case {
    ng_grid *grid;
    ng_puzzle *puzzle;
    ng_dim row; /* a random cell */
    ng_dim col;
} random_case;

static random_case draw(ng_test_rng *rng)
{
    random_case c = {.grid = ng_test_random_grid(rng, max_side, false)};
    c.puzzle = ng_test_puzzle_from_grid(c.grid);
    c.row = (ng_dim)ng_test_rng_below(rng, ng_grid_height(c.grid));
    c.col = (ng_dim)ng_test_rng_below(rng, ng_grid_width(c.grid));
    return c;
}

static void release(random_case *c)
{
    ng_puzzle_destroy(c->puzzle);
    ng_grid_destroy(c->grid);
}

Test(prop_verify, p_v1_a_grid_satisfies_its_own_clues)
{
    const uint64_t seed = ng_test_seed();
    const uint64_t cases = ng_test_case_count(default_cases);
    ng_test_rng rng;
    ng_test_rng_seed(&rng, seed);

    for (uint64_t k = 0; k < cases; ++k) {
        random_case c = draw(&rng);

        const ng_verify_report report = verify(c.puzzle, c.grid);

        cr_assert_eq(report.verdict, NG_VERDICT_VALID, "case %" PRIu64 " (seed %" PRIu64 ")", k,
                     seed);
        release(&c);
    }
}

Test(prop_verify, p_v2_flipping_one_cell_invalidates_its_row_first)
{
    const uint64_t seed = ng_test_seed();
    const uint64_t cases = ng_test_case_count(default_cases);
    ng_test_rng rng;
    ng_test_rng_seed(&rng, seed);

    for (uint64_t k = 0; k < cases; ++k) {
        random_case c = draw(&rng);
        const ng_cell old = ng_grid_get(c.grid, c.row, c.col);
        ng_grid_set(c.grid, c.row, c.col, old == NG_CELL_FILLED ? NG_CELL_EMPTY : NG_CELL_FILLED);

        const ng_verify_report report = verify(c.puzzle, c.grid);

        /* The flipped row changes its number of filled cells, so its runs cannot match; rows
         * above it are untouched and rows come first in the unified order. */
        cr_assert_eq(report.verdict, NG_VERDICT_INVALID, "case %" PRIu64 " (seed %" PRIu64 ")", k,
                     seed);
        cr_assert_eq(report.first_bad_line, c.row, "case %" PRIu64 " (seed %" PRIu64 ")", k, seed);
        release(&c);
    }
}

Test(prop_verify, p_v3_one_unknown_cell_makes_the_grid_incomplete)
{
    const uint64_t seed = ng_test_seed();
    const uint64_t cases = ng_test_case_count(default_cases);
    ng_test_rng rng;
    ng_test_rng_seed(&rng, seed);

    for (uint64_t k = 0; k < cases; ++k) {
        random_case c = draw(&rng);
        ng_grid_set(c.grid, c.row, c.col, NG_CELL_UNKNOWN);

        const ng_verify_report report = verify(c.puzzle, c.grid);

        cr_assert_eq(report.verdict, NG_VERDICT_INCOMPLETE, "case %" PRIu64 " (seed %" PRIu64 ")",
                     k, seed);
        cr_assert_eq(report.unknown_cells, 1u);
        release(&c);
    }
}

Test(prop_verify, p_v4_transposed_clues_accept_the_transposed_grid)
{
    const uint64_t seed = ng_test_seed();
    const uint64_t cases = ng_test_case_count(default_cases);
    ng_test_rng rng;
    ng_test_rng_seed(&rng, seed);

    for (uint64_t k = 0; k < cases; ++k) {
        random_case c = draw(&rng);
        ng_grid *grid_t = ng_test_transpose_grid(c.grid);
        ng_puzzle *puzzle_t = ng_test_transpose_puzzle(c.puzzle);

        const ng_verify_report report = verify(puzzle_t, grid_t);

        cr_assert_eq(report.verdict, NG_VERDICT_VALID, "case %" PRIu64 " (seed %" PRIu64 ")", k,
                     seed);
        ng_puzzle_destroy(puzzle_t);
        ng_grid_destroy(grid_t);
        release(&c);
    }
}

Test(prop_verify, observed_runs_of_a_solution_are_its_clues)
{
    const uint64_t seed = ng_test_seed();
    const uint64_t cases = ng_test_case_count(default_cases);
    ng_test_rng rng;
    ng_test_rng_seed(&rng, seed);

    for (uint64_t k = 0; k < cases; ++k) {
        random_case c = draw(&rng);
        for (ng_line_id line = 0; line < ng_puzzle_line_count(c.puzzle); ++line) {
            ng_dim observed[max_side] = {};
            ng_dim expected[max_side] = {};
            ng_run_count count = 0;
            cr_assert_eq(ng_verify_line_runs(c.grid, line, observed, max_side, &count), NG_OK);
            cr_assert_eq(ng_puzzle_copy_line_runs(c.puzzle, line, expected, max_side), NG_OK);
            cr_assert_eq(count, ng_puzzle_line_run_count(c.puzzle, line));
            cr_assert_arr_eq(observed, expected, count * sizeof(ng_dim),
                             "case %" PRIu64 ", line %u (seed %" PRIu64 ")", k, line, seed);
        }
        release(&c);
    }
}
