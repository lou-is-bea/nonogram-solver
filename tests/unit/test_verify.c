#include <criterion/criterion.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "nonogram/grid.h"
#include "nonogram/puzzle.h"
#include "nonogram/status.h"
#include "nonogram/types.h"
#include "nonogram/verify.h"

#include "ng_test_fixture.h"
#include "test_puzzles.h"

TestSuite(verify, .init = ng_test_setup, .fini = ng_test_teardown);
TestSuite(verify_line_runs, .init = ng_test_setup, .fini = ng_test_teardown);

/* The "P" of docs/FILE_FORMATS.md. */
static const char *const p_rows[] = {"###..", "#.#..", "###..", "#....", "#...."};

/* Verifies @p rows against the puzzle of @p solution_rows (both @p height rows). */
static ng_verify_report verify_rows(const char *const *solution_rows, const char *const *rows,
                                    ng_dim height)
{
    ng_grid *solution = ng_test_grid_of(solution_rows, height);
    ng_puzzle *puzzle = ng_test_puzzle_from_grid(solution);
    ng_grid *grid = ng_test_grid_of(rows, height);
    ng_verify_report report;
    memset(&report, 0xA5, sizeof report); /* every field must be written */

    cr_assert_eq(ng_verify(puzzle, grid, &report), NG_OK);

    ng_grid_destroy(grid);
    ng_puzzle_destroy(puzzle);
    ng_grid_destroy(solution);
    return report;
}

static void assert_invalid_at(ng_verify_report report, ng_line_id line, ng_run_count run)
{
    cr_assert_eq(report.verdict, NG_VERDICT_INVALID);
    cr_assert_eq(report.first_bad_line, line);
    cr_assert_eq(report.first_bad_run, run);
    cr_assert_eq(report.unknown_cells, 0u);
}

/* --- verify --------------------------------------------------------------------------------- */

Test(verify, solution_is_valid)
{
    const ng_verify_report report = verify_rows(p_rows, p_rows, 5);

    cr_assert_eq(report.verdict, NG_VERDICT_VALID);
    cr_assert_eq(report.first_bad_line, 0u);
    cr_assert_eq(report.first_bad_run, 0u);
    cr_assert_eq(report.unknown_cells, 0u);
}

Test(verify, another_grid_with_the_same_clues_is_valid)
{
    /* Two solutions of the same puzzle: rows 1 1, columns 1 1. */
    static const char *const a[] = {"#.", ".#"};
    static const char *const b[] = {".#", "#."};

    cr_assert_eq(verify_rows(a, b, 2).verdict, NG_VERDICT_VALID);
}

Test(verify, unknown_cells_make_the_grid_incomplete)
{
    static const char *const one_unknown[] = {"###..", "#.#..", "###..", "#...?", "#...."};
    static const char *const all_unknown[] = {"?????", "?????", "?????", "?????", "?????"};

    const ng_verify_report one = verify_rows(p_rows, one_unknown, 5);
    const ng_verify_report all = verify_rows(p_rows, all_unknown, 5);

    cr_assert_eq(one.verdict, NG_VERDICT_INCOMPLETE);
    cr_assert_eq(one.unknown_cells, 1u);
    cr_assert_eq(one.first_bad_line, 0u);
    cr_assert_eq(all.verdict, NG_VERDICT_INCOMPLETE);
    cr_assert_eq(all.unknown_cells, 25u);
}

Test(verify, unknown_cells_take_precedence_over_contradictions)
{
    static const char *const wrong_and_unknown[] = {"#####", "#.#..", "###..", "#...?", "#...."};

    const ng_verify_report report = verify_rows(p_rows, wrong_and_unknown, 5);

    cr_assert_eq(report.verdict, NG_VERDICT_INCOMPLETE);
    cr_assert_eq(report.unknown_cells, 1u);
}

Test(verify, wrong_run_length_is_reported_with_its_index)
{
    static const char *const rows[] = {"###..", "#.##.", "###..", "#....", "#...."};

    assert_invalid_at(verify_rows(p_rows, rows, 5), 1, 1);
}

Test(verify, mismatch_only_in_a_column_is_found_after_the_rows)
{
    /* Rows 1 and 1 in both grids; columns 2 0 expected, 1 1 drawn. */
    static const char *const solution[] = {"#.", "#."};
    static const char *const rows[] = {"#.", ".#"};

    assert_invalid_at(verify_rows(solution, rows, 2), 2, 0);
}

Test(verify, extra_and_missing_runs_point_past_the_common_prefix)
{
    static const char *const solution[] = {"###.."};
    static const char *const extra[] = {"###.#"};
    static const char *const one_run[] = {"#.#.."};
    static const char *const two_runs[] = {"#...."};

    assert_invalid_at(verify_rows(solution, extra, 1), 0, 1);
    /* Clue 1 1, observed 1: the second run is missing. */
    assert_invalid_at(verify_rows(one_run, two_runs, 1), 0, 1);
}

Test(verify, filled_cells_on_an_empty_line_are_reported_at_run_zero)
{
    static const char *const solution[] = {"..", "#."};
    static const char *const rows[] = {"#.", "#."};

    assert_invalid_at(verify_rows(solution, rows, 2), 0, 0);
}

Test(verify, runs_touching_the_borders_are_closed)
{
    static const char *const solution[] = {"#.#", "#.#"};

    cr_assert_eq(verify_rows(solution, solution, 2).verdict, NG_VERDICT_VALID);
}

Test(verify, one_by_one_grids)
{
    static const char *const filled[] = {"#"};
    static const char *const empty[] = {"."};

    cr_assert_eq(verify_rows(filled, filled, 1).verdict, NG_VERDICT_VALID);
    cr_assert_eq(verify_rows(empty, empty, 1).verdict, NG_VERDICT_VALID);
    assert_invalid_at(verify_rows(filled, empty, 1), 0, 0);
    assert_invalid_at(verify_rows(empty, filled, 1), 0, 0);
}

Test(verify, very_long_line_with_many_runs_is_valid)
{
    /* One row of 10^6 cells alternating filled and empty: 500 000 runs. */
    constexpr ng_dim width = 1'000'000;
    ng_grid *grid = nullptr;
    cr_assert_eq(ng_grid_create(&ng_test_heap()->base, width, 1, &grid), NG_OK);
    for (ng_dim c = 0; c < width; ++c) {
        ng_grid_set(grid, 0, c, c % 2u == 0u ? NG_CELL_FILLED : NG_CELL_EMPTY);
    }
    ng_puzzle *puzzle = ng_test_puzzle_from_grid(grid);
    ng_verify_report report = {};

    cr_assert_eq(ng_verify(puzzle, grid, &report), NG_OK);

    cr_assert_eq(report.verdict, NG_VERDICT_VALID);
    ng_puzzle_destroy(puzzle);
    ng_grid_destroy(grid);
}

Test(verify, mismatched_dimensions_are_invalid_and_leave_the_report)
{
    ng_grid *solution = ng_test_grid_of(p_rows, 5);
    ng_puzzle *puzzle = ng_test_puzzle_from_grid(solution);
    static const char *const transposed_shape[] = {"##", "##", "##"};
    ng_grid *other = ng_test_grid_of(transposed_shape, 3);
    ng_verify_report report = {.verdict = NG_VERDICT_INCOMPLETE, .unknown_cells = 7};

    const ng_status status = ng_verify(puzzle, other, &report);

    cr_assert_eq(status, NG_ERR_INVALID_ARGUMENT);
    cr_assert_eq(report.verdict, NG_VERDICT_INCOMPLETE);
    cr_assert_eq(report.unknown_cells, 7u);
    ng_grid_destroy(other);
    ng_puzzle_destroy(puzzle);
    ng_grid_destroy(solution);
}

Test(verify, nullptr_arguments_are_invalid)
{
    ng_grid *grid = ng_test_grid_of(p_rows, 5);
    ng_puzzle *puzzle = ng_test_puzzle_from_grid(grid);
    ng_verify_report report = {};

    cr_assert_eq(ng_verify(nullptr, grid, &report), NG_ERR_INVALID_ARGUMENT);
    cr_assert_eq(ng_verify(puzzle, nullptr, &report), NG_ERR_INVALID_ARGUMENT);
    cr_assert_eq(ng_verify(puzzle, grid, nullptr), NG_ERR_INVALID_ARGUMENT);
    ng_puzzle_destroy(puzzle);
    ng_grid_destroy(grid);
}

/* --- verify_line_runs ----------------------------------------------------------------------- */

Test(verify_line_runs, row_and_column_runs_in_reading_order)
{
    ng_grid *grid = ng_test_grid_of(p_rows, 5);
    ng_dim runs[3] = {};
    ng_run_count count = 0;

    cr_assert_eq(ng_verify_line_runs(grid, 1, runs, 3, &count), NG_OK);
    cr_assert_eq(count, 2u);
    cr_assert_eq(runs[0], 1u);
    cr_assert_eq(runs[1], 1u);
    cr_assert_eq(ng_verify_line_runs(grid, 5 + 2, runs, 3, &count), NG_OK);
    cr_assert_eq(count, 1u);
    cr_assert_eq(runs[0], 3u);
    ng_grid_destroy(grid);
}

Test(verify_line_runs, small_capacity_still_reports_the_full_count)
{
    static const char *const rows[] = {"#.##.#"};
    ng_grid *grid = ng_test_grid_of(rows, 1);
    ng_dim runs[1] = {};
    ng_run_count count = 0;

    cr_assert_eq(ng_verify_line_runs(grid, 0, runs, 1, &count), NG_OK);
    cr_assert_eq(count, 3u);
    cr_assert_eq(runs[0], 1u);
    cr_assert_eq(ng_verify_line_runs(grid, 0, nullptr, 0, &count), NG_OK);
    cr_assert_eq(count, 3u);
    ng_grid_destroy(grid);
}

Test(verify_line_runs, unknown_cells_count_as_not_filled)
{
    static const char *const rows[] = {"#?#", "???"};
    ng_grid *grid = ng_test_grid_of(rows, 2);
    ng_dim runs[2] = {};
    ng_run_count count = 0;

    cr_assert_eq(ng_verify_line_runs(grid, 0, runs, 2, &count), NG_OK);
    cr_assert_eq(count, 2u);
    cr_assert_eq(ng_verify_line_runs(grid, 1, runs, 2, &count), NG_OK);
    cr_assert_eq(count, 0u);
    ng_grid_destroy(grid);
}

Test(verify_line_runs, invalid_arguments_are_rejected)
{
    ng_grid *grid = ng_test_grid_of(p_rows, 5);
    ng_dim runs[3] = {};
    ng_run_count count = 0;

    cr_assert_eq(ng_verify_line_runs(nullptr, 0, runs, 3, &count), NG_ERR_INVALID_ARGUMENT);
    cr_assert_eq(ng_verify_line_runs(grid, 10, runs, 3, &count), NG_ERR_INVALID_ARGUMENT);
    cr_assert_eq(ng_verify_line_runs(grid, 0, runs, 3, nullptr), NG_ERR_INVALID_ARGUMENT);
    cr_assert_eq(ng_verify_line_runs(grid, 0, nullptr, 3, &count), NG_ERR_INVALID_ARGUMENT);
    ng_grid_destroy(grid);
}
