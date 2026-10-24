#include <criterion/criterion.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>

#include "nonogram/puzzle.h"
#include "nonogram/status.h"
#include "nonogram/types.h"

#include "ng_test_fixture.h"
#include "test_allocator.h"

TestSuite(puzzle_create, .init = ng_test_setup, .fini = ng_test_teardown);
TestSuite(puzzle_lines, .init = ng_test_setup, .fini = ng_test_teardown);

/* The "P" of docs/FILE_FORMATS.md §1.1, 5 x 5:
 *   ###..   rows: 3 | 1 1 | 3 | 1 | 1
 *   #.#..   columns: 5 | 1 1 | 3 | - | -
 *   ###..
 *   #....
 *   #....
 */
static const ng_dim p_runs[] = {3, 1, 1, 3, 1, 1, 5, 1, 1, 3};
static const ng_run_index p_offsets[] = {0, 1, 3, 4, 5, 6, 7, 9, 10, 10, 10};

static ng_puzzle *make_p(void)
{
    ng_puzzle *puzzle = nullptr;
    cr_assert_eq(ng_puzzle_create(&ng_test_heap()->base, 5, 5, p_runs, p_offsets, &puzzle), NG_OK);
    return puzzle;
}

static ng_status create(ng_dim width, ng_dim height, const ng_dim *runs,
                        const ng_run_index *offsets)
{
    ng_puzzle *puzzle = (ng_puzzle *)&puzzle;
    const ng_status status =
        ng_puzzle_create(&ng_test_heap()->base, width, height, runs, offsets, &puzzle);
    if (status == NG_OK) {
        ng_puzzle_destroy(puzzle);
    } else {
        cr_assert_null(puzzle, "out is reset on failure");
    }
    return status;
}

/* --- puzzle_create ------------------------------------------------------------------------ */

Test(puzzle_create, p_has_its_dimensions_and_run_total)
{
    ng_puzzle *puzzle = make_p();

    cr_assert_eq(ng_puzzle_width(puzzle), 5u);
    cr_assert_eq(ng_puzzle_height(puzzle), 5u);
    cr_assert_eq(ng_puzzle_line_count(puzzle), 10u);
    cr_assert_eq(ng_puzzle_total_runs(puzzle), 10u);
    ng_puzzle_destroy(puzzle);
}

Test(puzzle_create, input_arrays_are_copied)
{
    ng_dim runs[] = {1, 1};
    ng_run_index offsets[] = {0, 1, 2};
    ng_puzzle *puzzle = nullptr;
    cr_assert_eq(ng_puzzle_create(&ng_test_heap()->base, 1, 1, runs, offsets, &puzzle), NG_OK);

    runs[0] = 9;
    offsets[1] = 0;

    ng_dim clue[1] = {};
    cr_assert_eq(ng_puzzle_line_run_count(puzzle, 0), 1u);
    cr_assert_eq(ng_puzzle_copy_line_runs(puzzle, 0, clue, 1), NG_OK);
    cr_assert_eq(clue[0], 1u);
    ng_puzzle_destroy(puzzle);
}

Test(puzzle_create, one_by_one_empty_and_full_are_valid)
{
    const ng_run_index empty_offsets[] = {0, 0, 0};
    const ng_dim full_runs[] = {1, 1};
    const ng_run_index full_offsets[] = {0, 1, 2};

    cr_assert_eq(create(1, 1, nullptr, empty_offsets), NG_OK);
    cr_assert_eq(create(1, 1, full_runs, full_offsets), NG_OK);
}

Test(puzzle_create, clue_filling_the_line_exactly_is_valid)
{
    /* 6 x 1: the row "##.#.#" has clue 2 1 1, which needs 2 + 1 + 1 + 1 + 1 = 6 cells: no
     * freedom of placement. */
    const ng_dim runs[] = {2, 1, 1, 1, 1, 1, 1};
    const ng_run_index offsets[] = {0, 3, 4, 5, 5, 6, 6, 7};

    cr_assert_eq(create(6, 1, runs, offsets), NG_OK);
}

Test(puzzle_create, full_line_clue_is_valid)
{
    /* 3 x 2: rows "###" and "..."; every column is a single 1. */
    const ng_dim runs[] = {3, 1, 1, 1};
    const ng_run_index offsets[] = {0, 1, 1, 2, 3, 4};

    cr_assert_eq(create(3, 2, runs, offsets), NG_OK);
}

Test(puzzle_create, non_square_puzzles_have_line_lengths_by_orientation)
{
    /* 3 wide x 2 tall, as above. */
    const ng_dim runs[] = {3, 1, 1, 1};
    const ng_run_index offsets[] = {0, 1, 1, 2, 3, 4};
    ng_puzzle *puzzle = nullptr;
    cr_assert_eq(ng_puzzle_create(&ng_test_heap()->base, 3, 2, runs, offsets, &puzzle), NG_OK);

    cr_assert_eq(ng_puzzle_line_length(puzzle, 0), 3u, "rows span the width");
    cr_assert_eq(ng_puzzle_line_length(puzzle, 1), 3u);
    cr_assert_eq(ng_puzzle_line_length(puzzle, 2), 2u, "columns span the height");
    cr_assert_eq(ng_puzzle_line_length(puzzle, 4), 2u);
    ng_puzzle_destroy(puzzle);
}

Test(puzzle_create, nullptr_arguments_are_invalid)
{
    ng_puzzle *puzzle = (ng_puzzle *)&puzzle;

    cr_assert_eq(ng_puzzle_create(nullptr, 5, 5, p_runs, p_offsets, &puzzle),
                 NG_ERR_INVALID_ARGUMENT);
    cr_assert_null(puzzle);
    cr_assert_eq(ng_puzzle_create(&ng_test_heap()->base, 5, 5, p_runs, p_offsets, nullptr),
                 NG_ERR_INVALID_ARGUMENT);
    cr_assert_eq(create(5, 5, p_runs, nullptr), NG_ERR_INVALID_ARGUMENT);
    cr_assert_eq(create(5, 5, nullptr, p_offsets), NG_ERR_INVALID_ARGUMENT,
                 "runs are required when the offsets announce some");
}

Test(puzzle_create, dimension_out_of_range_returns_limit)
{
    /* The offsets are never read: the dimensions are rejected first. */
    const ng_run_index offsets[] = {0};

    cr_assert_eq(create(0, 5, nullptr, offsets), NG_ERR_LIMIT);
    cr_assert_eq(create(5, 0, nullptr, offsets), NG_ERR_LIMIT);
    cr_assert_eq(create(NG_DIM_MAX + 1u, 1, nullptr, offsets), NG_ERR_LIMIT);
    cr_assert_eq(create(1, NG_DIM_MAX + 1u, nullptr, offsets), NG_ERR_LIMIT);
}

Test(puzzle_create, offsets_must_start_at_zero_and_not_decrease)
{
    const ng_dim runs[] = {1, 1};
    const ng_run_index shifted[] = {1, 1, 2};
    const ng_run_index decreasing[] = {0, 2, 1};

    cr_assert_eq(create(1, 1, runs, shifted), NG_ERR_INVALID_ARGUMENT);
    cr_assert_eq(create(1, 1, runs, decreasing), NG_ERR_INVALID_ARGUMENT);
}

Test(puzzle_create, too_many_runs_is_inconsistent_without_reading_them)
{
    /* The column claims 2^63 runs: rejected from the offsets, the runs are never read. */
    const ng_dim runs[] = {1};
    const ng_run_index offsets[] = {0, 1, (ng_run_index)1 << 63};

    cr_assert_eq(create(1, 1, runs, offsets), NG_ERR_INCONSISTENT);
}

Test(puzzle_create, zero_length_run_is_invalid)
{
    const ng_dim runs[] = {0, 1};
    const ng_run_index offsets[] = {0, 1, 2};

    cr_assert_eq(create(1, 1, runs, offsets), NG_ERR_INVALID_ARGUMENT);
}

Test(puzzle_create, run_longer_than_its_line_is_inconsistent)
{
    /* 2 x 1: the row clue 3 cannot fit in 2 cells. */
    const ng_dim runs[] = {3, 1, 1};
    const ng_run_index offsets[] = {0, 1, 2, 3};

    cr_assert_eq(create(2, 1, runs, offsets), NG_ERR_INCONSISTENT);
}

Test(puzzle_create, runs_and_gaps_exceeding_the_line_are_inconsistent)
{
    /* 5 x 1: row 3 2 needs 3 + 1 + 2 = 6 cells. Column totals match the rows (5). */
    const ng_dim runs[] = {3, 2, 1, 1, 1, 1, 1};
    const ng_run_index offsets[] = {0, 2, 3, 4, 5, 6, 7};

    cr_assert_eq(create(5, 1, runs, offsets), NG_ERR_INCONSISTENT);
}

Test(puzzle_create, different_row_and_column_totals_are_inconsistent)
{
    /* 2 x 2: rows say 1 + 1 = 2 filled cells, columns say 2 + 1 = 3. */
    const ng_dim runs[] = {1, 1, 2, 1};
    const ng_run_index offsets[] = {0, 1, 2, 3, 4};

    cr_assert_eq(create(2, 2, runs, offsets), NG_ERR_INCONSISTENT);
}

Test(puzzle_create, every_allocation_failure_is_clean)
{
    ng_test_allocator *ta = ng_test_heap();
    for (ng_counter n = 0;; ++n) {
        ng_test_allocator_fail_at(ta, n);
        ng_puzzle *puzzle = nullptr;

        const ng_status status = ng_puzzle_create(&ta->base, 5, 5, p_runs, p_offsets, &puzzle);

        if (status == NG_OK) {
            ng_puzzle_destroy(puzzle);
            break;
        }
        cr_assert_eq(status, NG_ERR_OUT_OF_MEMORY);
        cr_assert_null(puzzle);
        cr_assert_eq(ta->live_bytes, 0u);
    }
}

Test(puzzle_create, destroy_of_nullptr_does_nothing)
{
    ng_puzzle_destroy(nullptr);
}

/* --- puzzle_lines ------------------------------------------------------------------------- */

Test(puzzle_lines, run_counts_follow_the_clues)
{
    ng_puzzle *puzzle = make_p();
    const ng_run_count expected[] = {1, 2, 1, 1, 1, 1, 2, 1, 0, 0};

    for (ng_line_id line = 0; line < 10; ++line) {
        cr_assert_eq(ng_puzzle_line_run_count(puzzle, line), expected[line], "line %u", line);
    }
    ng_puzzle_destroy(puzzle);
}

Test(puzzle_lines, row_and_column_clues_are_copied_in_order)
{
    ng_puzzle *puzzle = make_p();
    ng_dim row[2] = {};
    ng_dim column[2] = {};

    cr_assert_eq(ng_puzzle_copy_line_runs(puzzle, 1, row, 2), NG_OK);
    cr_assert_eq(ng_puzzle_copy_line_runs(puzzle, 5 + 0, column, 2), NG_OK);

    cr_assert_eq(row[0], 1u);
    cr_assert_eq(row[1], 1u);
    cr_assert_eq(column[0], 5u);
    ng_puzzle_destroy(puzzle);
}

Test(puzzle_lines, empty_line_accepts_a_nullptr_buffer)
{
    ng_puzzle *puzzle = make_p();

    const ng_status status = ng_puzzle_copy_line_runs(puzzle, 5 + 3, nullptr, 0);

    cr_assert_eq(status, NG_OK);
    ng_puzzle_destroy(puzzle);
}

Test(puzzle_lines, too_small_capacity_is_invalid_and_leaves_the_buffer)
{
    ng_puzzle *puzzle = make_p();
    ng_dim row[2] = {42, 42};

    const ng_status status = ng_puzzle_copy_line_runs(puzzle, 1, row, 1);

    cr_assert_eq(status, NG_ERR_INVALID_ARGUMENT);
    cr_assert_eq(row[0], 42u);
    ng_puzzle_destroy(puzzle);
}

Test(puzzle_lines, invalid_copy_arguments_are_rejected)
{
    ng_puzzle *puzzle = make_p();
    ng_dim row[2] = {};

    cr_assert_eq(ng_puzzle_copy_line_runs(nullptr, 0, row, 2), NG_ERR_INVALID_ARGUMENT);
    cr_assert_eq(ng_puzzle_copy_line_runs(puzzle, 10, row, 2), NG_ERR_INVALID_ARGUMENT);
    cr_assert_eq(ng_puzzle_copy_line_runs(puzzle, 1, nullptr, 2), NG_ERR_INVALID_ARGUMENT);
    ng_puzzle_destroy(puzzle);
}

Test(puzzle_lines, accessors_treat_nullptr_as_empty)
{
    cr_assert_eq(ng_puzzle_width(nullptr), 0u);
    cr_assert_eq(ng_puzzle_height(nullptr), 0u);
    cr_assert_eq(ng_puzzle_line_count(nullptr), 0u);
    cr_assert_eq(ng_puzzle_total_runs(nullptr), 0u);
}

#ifndef NDEBUG
Test(puzzle_lines, line_length_out_of_range_asserts, .signal = SIGABRT)
{
    ng_puzzle *puzzle = make_p();

    ng_puzzle_line_length(puzzle, 10);
}

Test(puzzle_lines, run_count_on_nullptr_asserts, .signal = SIGABRT)
{
    ng_puzzle_line_run_count(nullptr, 0);
}
#else
/* Release builds: the documented fallbacks of contract violations (run with the release
 * preset). */
Test(puzzle_lines, release_invalid_line_queries_return_zero)
{
    ng_puzzle *puzzle = make_p();

    cr_assert_eq(ng_puzzle_line_length(puzzle, 10), 0u);
    cr_assert_eq(ng_puzzle_line_run_count(puzzle, 10), 0u);
    cr_assert_eq(ng_puzzle_line_length(nullptr, 0), 0u);
    cr_assert_eq(ng_puzzle_line_run_count(nullptr, 0), 0u);
    ng_puzzle_destroy(puzzle);
}
#endif
