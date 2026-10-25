#include <criterion/criterion.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "nonogram/grid.h"
#include "nonogram/io.h"
#include "nonogram/puzzle.h"
#include "nonogram/status.h"
#include "nonogram/stream.h"
#include "nonogram/types.h"
#include "nonogram/verify.h"

#include "ng_test_fixture.h"
#include "test_allocator.h"

/* First tier of the massive tests (TESTING.md §4.6, tests/massive/README.md): 10^4 x 10^4
 * grids, i.e. 10^8 cells, plus representation limits checked with the capped allocator. The
 * grids are generated, never stored. Label "massive", preset "massive" (Release). */
TestSuite(massive, .init = ng_test_setup, .fini = ng_test_teardown, .timeout = 600);

static constexpr ng_dim side = 10'000;
static constexpr ng_dim stripe = 50; /* diagonal stripes of 50 filled then 50 empty cells */
static constexpr double formula_tolerance = 0.05;

/* About 100 runs per line, 2 x 10^6 runs in total: realistic clue volume, cheap to check. */
static ng_cell pattern(ng_dim row, ng_dim col)
{
    return (((uint64_t)row + col) % ((uint64_t)2 * stripe)) < stripe ? NG_CELL_FILLED
                                                                     : NG_CELL_EMPTY;
}

static ng_cell line_buffer[side];
static ng_cell expected_line[side];

static void fill_expected(ng_line_id line)
{
    for (ng_dim i = 0; i < side; ++i) {
        expected_line[i] = line < side ? pattern(line, i) : pattern(i, line - side);
    }
}

static ng_grid *patterned_grid(void)
{
    ng_grid *grid = nullptr;
    cr_assert_eq(ng_grid_create(&ng_test_heap()->base, side, side, &grid), NG_OK);
    for (ng_line_id row = 0; row < side; ++row) {
        fill_expected(row);
        cr_assert_eq(ng_grid_store_line(grid, row, expected_line), NG_OK);
    }
    return grid;
}

static void assert_within_formula(size_t measured, double expected, const char *what)
{
    const double ratio = (double)measured / expected;
    cr_assert(ratio >= 1.0 - formula_tolerance && ratio <= 1.0 + formula_tolerance,
              "%s: measured %zu bytes, formula %.0f (ratio %.4f)", what, measured, expected, ratio);
}

static void assert_every_line_matches(const ng_grid *grid)
{
    for (ng_line_id line = 0; line < 2u * side; ++line) {
        fill_expected(line);
        cr_assert_eq(ng_grid_load_line(grid, line, line_buffer), NG_OK);
        if (memcmp(line_buffer, expected_line, side) != 0) { /* square grids */
            cr_assert_fail("line %u differs", line);
        }
    }
}

Test(massive, grid_of_10_power_8_cells_matches_its_memory_formula_and_round_trips_lines)
{
    const size_t before = ng_test_heap()->live_bytes;

    ng_grid *grid = patterned_grid();

    /* NF-05: one byte per cell plus a constant header. */
    assert_within_formula(ng_test_heap()->live_bytes - before, (double)side * side, "grid");
    assert_every_line_matches(grid);
    ng_grid_destroy(grid);
}

Test(massive, grid_of_10_power_10_cells_is_requested_whole_and_refused_cleanly)
{
    ng_test_allocator *ta = ng_test_heap();
    ng_test_allocator_cap(ta, (size_t)1 << 30);
    ng_grid *grid = nullptr;

    const ng_status status = ng_grid_create(&ta->base, 100'000, 100'000, &grid);

    cr_assert_eq(status, NG_ERR_OUT_OF_MEMORY);
    cr_assert_null(grid);
    cr_assert_geq((uint64_t)ta->last_request_size, (uint64_t)10'000'000'000u);
    cr_assert_eq(ta->live_bytes, 0u);
}

/* Builds the puzzle of the patterned grid line by line, as a streaming generator would. */
static ng_puzzle *patterned_puzzle(const ng_grid *grid)
{
    const ng_allocator *a = &ng_test_heap()->base;
    const size_t line_count = 2u * (size_t)side;
    const size_t max_runs = line_count * ((side / 2u) + 1u);
    ng_run_index *offsets = a->alloc(a->ctx, (line_count + 1u) * sizeof *offsets, 8);
    ng_dim *runs = a->alloc(a->ctx, max_runs * sizeof *runs, 4);
    cr_assert(offsets != nullptr && runs != nullptr);
    ng_run_index count = 0;
    offsets[0] = 0;
    for (ng_line_id line = 0; line < line_count; ++line) {
        cr_assert_eq(ng_grid_load_line(grid, line, line_buffer), NG_OK);
        ng_dim run = 0;
        for (ng_dim i = 0; i <= side; ++i) {
            if (i < side && line_buffer[i] == NG_CELL_FILLED) {
                ++run;
            } else if (run != 0u) {
                runs[count++] = run;
                run = 0;
            }
        }
        offsets[line + 1u] = count;
    }
    ng_puzzle *puzzle = nullptr;
    cr_assert_eq(ng_puzzle_create(a, side, side, runs, offsets, &puzzle), NG_OK);
    a->free(a->ctx, offsets, (line_count + 1u) * sizeof *offsets);
    a->free(a->ctx, runs, max_runs * sizeof *runs);
    return puzzle;
}

Test(massive, puzzle_of_two_million_runs_matches_its_memory_formula)
{
    ng_grid *grid = patterned_grid();
    const size_t before = ng_test_heap()->live_bytes;

    ng_puzzle *puzzle = patterned_puzzle(grid);

    const size_t measured = ng_test_heap()->live_bytes - before;
    const double lines = 2.0 * side;
    const double formula = ((lines + 1.0) * (double)sizeof(ng_run_index)) +
                           ((double)ng_puzzle_total_runs(puzzle) * (double)sizeof(ng_dim));
    cr_assert_gt(ng_puzzle_total_runs(puzzle), 1'000'000u);
    assert_within_formula(measured, formula, "puzzle");
    ng_puzzle_destroy(puzzle);
    ng_grid_destroy(grid);
}

Test(massive, non_round_trip_of_two_million_runs)
{
    ng_grid *grid = patterned_grid();
    ng_puzzle *puzzle = patterned_puzzle(grid);
    ng_grid_destroy(grid);
    ng_buffer *text = nullptr;
    cr_assert_eq(ng_buffer_create(&ng_test_heap()->base, &text), NG_OK);
    const ng_writer w = ng_writer_from_buffer(text);
    cr_assert_eq(ng_io_write_non(puzzle, nullptr, &w), NG_OK);
    ng_memory_source source;
    const ng_reader r = ng_reader_from_memory(&source, ng_buffer_data(text), ng_buffer_size(text));
    ng_puzzle *back = nullptr;
    ng_parse_error error = {};

    const ng_status status = ng_io_read_non(&ng_test_heap()->base, &r, &back, nullptr, &error);

    cr_assert_eq(status, NG_OK, "%s", error.message);
    cr_assert_eq(ng_puzzle_total_runs(back), ng_puzzle_total_runs(puzzle));
    /* A line of side cells holds at most side / 2 + 1 runs. */
    static ng_dim runs_expected[(side / 2u) + 1u];
    static ng_dim runs_back[(side / 2u) + 1u];
    constexpr ng_run_count capacity = (side / 2u) + 1u;
    for (ng_line_id line = 0; line < ng_puzzle_line_count(puzzle); ++line) {
        const ng_run_count count = ng_puzzle_line_run_count(puzzle, line);
        if (ng_puzzle_line_run_count(back, line) != count ||
            ng_puzzle_copy_line_runs(puzzle, line, runs_expected, capacity) != NG_OK ||
            ng_puzzle_copy_line_runs(back, line, runs_back, capacity) != NG_OK ||
            memcmp(runs_expected, runs_back, count * sizeof(ng_dim)) != 0) {
            cr_assert_fail("line %u: runs differ", line);
        }
    }
    ng_puzzle_destroy(back);
    ng_buffer_destroy(text);
    ng_puzzle_destroy(puzzle);
}

Test(massive, sol_round_trip_of_10_power_8_cells)
{
    ng_grid *grid = patterned_grid();
    ng_buffer *text = nullptr;
    cr_assert_eq(ng_buffer_create(&ng_test_heap()->base, &text), NG_OK);
    const ng_writer w = ng_writer_from_buffer(text);
    cr_assert_eq(ng_io_write_sol(grid, &w), NG_OK);
    ng_grid_destroy(grid);
    ng_memory_source source;
    const ng_reader r = ng_reader_from_memory(&source, ng_buffer_data(text), ng_buffer_size(text));
    ng_grid *back = nullptr;
    ng_parse_error error = {};

    const ng_status status = ng_io_read_sol(&ng_test_heap()->base, &r, &back, &error);

    cr_assert_eq(status, NG_OK, "%s", error.message);
    ng_buffer_destroy(text);
    assert_every_line_matches(back);
    ng_grid_destroy(back);
}

Test(massive, verifier_accepts_a_solution_of_10_power_8_cells_and_rejects_one_flip)
{
    ng_grid *grid = patterned_grid();
    ng_puzzle *puzzle = patterned_puzzle(grid);
    ng_verify_report report = {};

    cr_assert_eq(ng_verify(puzzle, grid, &report), NG_OK);
    cr_assert_eq(report.verdict, NG_VERDICT_VALID);

    /* The last cell: every row before it must be checked before the mismatch is found. */
    const ng_dim last = side - 1u;
    ng_grid_set(grid, last, last,
                ng_grid_get(grid, last, last) == NG_CELL_FILLED ? NG_CELL_EMPTY : NG_CELL_FILLED);
    cr_assert_eq(ng_verify(puzzle, grid, &report), NG_OK);
    cr_assert_eq(report.verdict, NG_VERDICT_INVALID);
    cr_assert_eq(report.first_bad_line, last);
    ng_puzzle_destroy(puzzle);
    ng_grid_destroy(grid);
}
