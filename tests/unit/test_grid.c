#include <criterion/criterion.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "nonogram/grid.h"
#include "nonogram/status.h"
#include "nonogram/types.h"

#include "ng_test_fixture.h"
#include "test_allocator.h"

TestSuite(grid_create, .init = ng_test_setup, .fini = ng_test_teardown);
TestSuite(grid_access, .init = ng_test_setup, .fini = ng_test_teardown);
TestSuite(grid_copy, .init = ng_test_setup, .fini = ng_test_teardown);
TestSuite(grid_line, .init = ng_test_setup, .fini = ng_test_teardown);

static ng_grid *make_grid(ng_dim width, ng_dim height)
{
    ng_grid *grid = nullptr;
    cr_assert_eq(ng_grid_create(&ng_test_heap()->base, width, height, &grid), NG_OK);
    return grid;
}

/* Fills cell (r, c) with a value that depends on both coordinates, so that any transposition
 * or offset error in line access shows up. */
static ng_cell pattern(ng_dim row, ng_dim col)
{
    static const ng_cell cycle[] = {NG_CELL_FILLED, NG_CELL_EMPTY, NG_CELL_UNKNOWN, NG_CELL_FILLED,
                                    NG_CELL_FILLED};
    return cycle[((size_t)row * 3u + (size_t)col) % 5u];
}

static ng_grid *make_patterned_grid(ng_dim width, ng_dim height)
{
    ng_grid *grid = make_grid(width, height);
    for (ng_dim r = 0; r < height; ++r) {
        for (ng_dim c = 0; c < width; ++c) {
            ng_grid_set(grid, r, c, pattern(r, c));
        }
    }
    return grid;
}

/* --- grid_create -------------------------------------------------------------------------- */

Test(grid_create, new_grid_has_its_dimensions_and_unknown_cells)
{
    ng_grid *grid = nullptr;

    const ng_status status = ng_grid_create(&ng_test_heap()->base, 7, 5, &grid);

    cr_assert_eq(status, NG_OK);
    cr_assert_eq(ng_grid_width(grid), 7u);
    cr_assert_eq(ng_grid_height(grid), 5u);
    for (ng_dim r = 0; r < 5; ++r) {
        for (ng_dim c = 0; c < 7; ++c) {
            cr_assert_eq(ng_grid_get(grid, r, c), NG_CELL_UNKNOWN);
        }
    }
    ng_grid_destroy(grid);
}

Test(grid_create, one_by_one_grid_holds_every_state)
{
    ng_grid *grid = make_grid(1, 1);
    const ng_cell states[] = {NG_CELL_EMPTY, NG_CELL_FILLED, NG_CELL_UNKNOWN};

    for (size_t i = 0; i < sizeof states / sizeof states[0]; ++i) {
        ng_grid_set(grid, 0, 0, states[i]);
        cr_assert_eq(ng_grid_get(grid, 0, 0), states[i]);
    }
    ng_grid_destroy(grid);
}

Test(grid_create, zero_dimension_returns_limit)
{
    ng_grid *grid = (ng_grid *)&grid;

    cr_assert_eq(ng_grid_create(&ng_test_heap()->base, 0, 5, &grid), NG_ERR_LIMIT);
    cr_assert_null(grid);
    cr_assert_eq(ng_grid_create(&ng_test_heap()->base, 5, 0, &grid), NG_ERR_LIMIT);
    cr_assert_null(grid);
}

Test(grid_create, dimension_above_dim_max_returns_limit)
{
    ng_grid *grid = nullptr;
    const ng_dim too_big = NG_DIM_MAX + 1u;

    cr_assert_eq(ng_grid_create(&ng_test_heap()->base, too_big, 1, &grid), NG_ERR_LIMIT);
    cr_assert_eq(ng_grid_create(&ng_test_heap()->base, 1, too_big, &grid), NG_ERR_LIMIT);
    cr_assert_eq(ng_grid_create(&ng_test_heap()->base, UINT32_MAX, 1, &grid), NG_ERR_LIMIT);
    cr_assert_null(grid);
    cr_assert_eq(ng_test_heap()->alloc_count, 0u);
}

Test(grid_create, nullptr_arguments_are_invalid)
{
    ng_grid *grid = (ng_grid *)&grid;

    cr_assert_eq(ng_grid_create(nullptr, 3, 3, &grid), NG_ERR_INVALID_ARGUMENT);
    cr_assert_null(grid);
    cr_assert_eq(ng_grid_create(&ng_test_heap()->base, 3, 3, nullptr), NG_ERR_INVALID_ARGUMENT);
}

Test(grid_create, every_allocation_failure_is_clean)
{
    ng_test_allocator *ta = ng_test_heap();
    for (ng_counter n = 0;; ++n) {
        ng_test_allocator_fail_at(ta, n);
        ng_grid *grid = nullptr;

        const ng_status status = ng_grid_create(&ta->base, 7, 5, &grid);

        if (status == NG_OK) {
            ng_grid_destroy(grid);
            break;
        }
        cr_assert_eq(status, NG_ERR_OUT_OF_MEMORY);
        cr_assert_null(grid);
        cr_assert_eq(ta->live_bytes, 0u);
    }
}

Test(grid_create, size_beyond_two_to_the_32_cells_is_computed_in_64_bits)
{
    /* 70 000 x 70 000 = 4.9e9 cells: a 32-bit product would wrap to about 605 million. */
    ng_test_allocator *ta = ng_test_heap();
    ng_test_allocator_cap(ta, (size_t)1 << 20);
    ng_grid *grid = nullptr;

    const ng_status status = ng_grid_create(&ta->base, 70'000, 70'000, &grid);

    cr_assert_eq(status, NG_ERR_OUT_OF_MEMORY);
    cr_assert_null(grid);
    cr_assert_geq(ta->last_request_size, (size_t)70'000 * 70'000);
}

Test(grid_create, dim_max_squared_is_requested_not_truncated)
{
    ng_test_allocator *ta = ng_test_heap();
    ng_test_allocator_cap(ta, (size_t)1 << 20);
    ng_grid *grid = nullptr;

    const ng_status status = ng_grid_create(&ta->base, NG_DIM_MAX, NG_DIM_MAX, &grid);

    cr_assert_eq(status, NG_ERR_OUT_OF_MEMORY);
    cr_assert_geq((ng_cell_index)ta->last_request_size, (ng_cell_index)NG_DIM_MAX * NG_DIM_MAX);
}

Test(grid_create, very_elongated_grids_are_supported)
{
    constexpr ng_dim long_side = 1'000'000;
    ng_grid *wide = make_grid(long_side, 1);
    ng_grid *tall = make_grid(1, long_side);

    ng_grid_set(wide, 0, long_side - 1u, NG_CELL_FILLED);
    ng_grid_set(tall, long_side - 1u, 0, NG_CELL_FILLED);

    cr_assert_eq(ng_grid_get(wide, 0, long_side - 1u), NG_CELL_FILLED);
    cr_assert_eq(ng_grid_get(wide, 0, long_side - 2u), NG_CELL_UNKNOWN);
    cr_assert_eq(ng_grid_get(tall, long_side - 1u, 0), NG_CELL_FILLED);
    ng_grid_destroy(wide);
    ng_grid_destroy(tall);
}

Test(grid_create, destroy_of_nullptr_does_nothing)
{
    ng_grid_destroy(nullptr);
}

/* --- grid_access -------------------------------------------------------------------------- */

Test(grid_access, set_changes_only_the_target_cell)
{
    ng_grid *grid = make_grid(4, 3);

    ng_grid_set(grid, 1, 2, NG_CELL_FILLED);

    for (ng_dim r = 0; r < 3; ++r) {
        for (ng_dim c = 0; c < 4; ++c) {
            const ng_cell expected = (r == 1 && c == 2) ? NG_CELL_FILLED : NG_CELL_UNKNOWN;
            cr_assert_eq(ng_grid_get(grid, r, c), expected, "cell (%u, %u)", r, c);
        }
    }
    ng_grid_destroy(grid);
}

Test(grid_access, fill_sets_every_cell)
{
    ng_grid *grid = make_patterned_grid(6, 4);

    ng_grid_fill(grid, NG_CELL_EMPTY);

    for (ng_dim r = 0; r < 4; ++r) {
        for (ng_dim c = 0; c < 6; ++c) {
            cr_assert_eq(ng_grid_get(grid, r, c), NG_CELL_EMPTY);
        }
    }
    ng_grid_destroy(grid);
}

Test(grid_access, dimension_accessors_treat_nullptr_as_empty)
{
    cr_assert_eq(ng_grid_width(nullptr), 0u);
    cr_assert_eq(ng_grid_height(nullptr), 0u);
}

#ifndef NDEBUG
/* Contract violations are internal assertions in Debug builds. */
Test(grid_access, get_outside_the_grid_asserts, .signal = SIGABRT)
{
    ng_grid *grid = make_grid(4, 3);

    ng_grid_get(grid, 3, 0);
}

Test(grid_access, set_outside_the_grid_asserts, .signal = SIGABRT)
{
    ng_grid *grid = make_grid(4, 3);

    ng_grid_set(grid, 0, 4, NG_CELL_FILLED);
}

Test(grid_access, set_of_a_non_enumerator_asserts, .signal = SIGABRT)
{
    ng_grid *grid = make_grid(4, 3);
    /* NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) */
    const ng_cell bogus = (ng_cell)3;

    ng_grid_set(grid, 0, 0, bogus);
}

Test(grid_access, get_on_nullptr_asserts, .signal = SIGABRT)
{
    ng_grid_get(nullptr, 0, 0);
}
#else
/* Release builds: the documented fallbacks of contract violations (run with the release
 * preset). */
Test(grid_access, release_get_outside_the_grid_returns_unknown)
{
    ng_grid *grid = make_patterned_grid(4, 3);

    cr_assert_eq(ng_grid_get(grid, 3, 0), NG_CELL_UNKNOWN);
    cr_assert_eq(ng_grid_get(nullptr, 0, 0), NG_CELL_UNKNOWN);
    ng_grid_destroy(grid);
}

Test(grid_access, release_invalid_set_and_fill_are_ignored)
{
    ng_grid *grid = make_grid(2, 2);
    /* NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) */
    const ng_cell bogus = (ng_cell)3;

    ng_grid_set(grid, 0, 2, NG_CELL_FILLED);
    ng_grid_set(grid, 0, 0, bogus);
    ng_grid_fill(grid, bogus);
    ng_grid_set(nullptr, 0, 0, NG_CELL_FILLED);
    ng_grid_fill(nullptr, NG_CELL_FILLED);

    for (ng_dim r = 0; r < 2; ++r) {
        for (ng_dim c = 0; c < 2; ++c) {
            cr_assert_eq(ng_grid_get(grid, r, c), NG_CELL_UNKNOWN);
        }
    }
    ng_grid_destroy(grid);
}
#endif

/* --- grid_copy ---------------------------------------------------------------------------- */

Test(grid_copy, copies_every_cell)
{
    ng_grid *src = make_patterned_grid(5, 3);
    ng_grid *dst = make_grid(5, 3);

    const ng_status status = ng_grid_copy(dst, src);

    cr_assert_eq(status, NG_OK);
    for (ng_dim r = 0; r < 3; ++r) {
        for (ng_dim c = 0; c < 5; ++c) {
            cr_assert_eq(ng_grid_get(dst, r, c), pattern(r, c));
        }
    }
    ng_grid_destroy(src);
    ng_grid_destroy(dst);
}

Test(grid_copy, different_dimensions_are_invalid_and_leave_dst_unchanged)
{
    ng_grid *src = make_patterned_grid(5, 3);
    ng_grid *transposed = make_grid(3, 5);

    const ng_status status = ng_grid_copy(transposed, src);

    cr_assert_eq(status, NG_ERR_INVALID_ARGUMENT);
    cr_assert_eq(ng_grid_get(transposed, 0, 0), NG_CELL_UNKNOWN);
    ng_grid_destroy(src);
    ng_grid_destroy(transposed);
}

Test(grid_copy, copy_onto_itself_is_a_no_op)
{
    ng_grid *grid = make_patterned_grid(5, 3);

    cr_assert_eq(ng_grid_copy(grid, grid), NG_OK);

    cr_assert_eq(ng_grid_get(grid, 2, 4), pattern(2, 4));
    ng_grid_destroy(grid);
}

Test(grid_copy, nullptr_arguments_are_invalid)
{
    ng_grid *grid = make_grid(2, 2);

    cr_assert_eq(ng_grid_copy(nullptr, grid), NG_ERR_INVALID_ARGUMENT);
    cr_assert_eq(ng_grid_copy(grid, nullptr), NG_ERR_INVALID_ARGUMENT);
    ng_grid_destroy(grid);
}

/* --- grid_line ---------------------------------------------------------------------------- */

Test(grid_line, row_loads_left_to_right)
{
    ng_grid *grid = make_patterned_grid(5, 3);
    ng_cell line[5] = {};

    const ng_status status = ng_grid_load_line(grid, 2, line);

    cr_assert_eq(status, NG_OK);
    for (ng_dim c = 0; c < 5; ++c) {
        cr_assert_eq(line[c], pattern(2, c), "column %u", c);
    }
    ng_grid_destroy(grid);
}

Test(grid_line, column_loads_top_to_bottom)
{
    ng_grid *grid = make_patterned_grid(5, 3);
    ng_cell line[3] = {};

    /* Columns follow the rows: line height + 4 is the last column. */
    const ng_status status = ng_grid_load_line(grid, 3 + 4, line);

    cr_assert_eq(status, NG_OK);
    for (ng_dim r = 0; r < 3; ++r) {
        cr_assert_eq(line[r], pattern(r, 4), "row %u", r);
    }
    ng_grid_destroy(grid);
}

Test(grid_line, stored_row_is_visible_cell_by_cell)
{
    ng_grid *grid = make_grid(4, 2);
    const ng_cell row[4] = {NG_CELL_FILLED, NG_CELL_EMPTY, NG_CELL_UNKNOWN, NG_CELL_FILLED};

    cr_assert_eq(ng_grid_store_line(grid, 1, row), NG_OK);

    for (ng_dim c = 0; c < 4; ++c) {
        cr_assert_eq(ng_grid_get(grid, 1, c), row[c]);
        cr_assert_eq(ng_grid_get(grid, 0, c), NG_CELL_UNKNOWN, "other row untouched");
    }
    ng_grid_destroy(grid);
}

Test(grid_line, stored_column_is_visible_cell_by_cell)
{
    ng_grid *grid = make_grid(4, 2);
    const ng_cell column[2] = {NG_CELL_EMPTY, NG_CELL_FILLED};

    cr_assert_eq(ng_grid_store_line(grid, 2 + 1, column), NG_OK);

    cr_assert_eq(ng_grid_get(grid, 0, 1), NG_CELL_EMPTY);
    cr_assert_eq(ng_grid_get(grid, 1, 1), NG_CELL_FILLED);
    cr_assert_eq(ng_grid_get(grid, 0, 0), NG_CELL_UNKNOWN, "other column untouched");
    cr_assert_eq(ng_grid_get(grid, 1, 2), NG_CELL_UNKNOWN, "other column untouched");
    ng_grid_destroy(grid);
}

Test(grid_line, line_out_of_range_is_invalid)
{
    ng_grid *grid = make_patterned_grid(4, 2);
    ng_cell line[4] = {NG_CELL_EMPTY, NG_CELL_EMPTY, NG_CELL_EMPTY, NG_CELL_EMPTY};

    cr_assert_eq(ng_grid_load_line(grid, 6, line), NG_ERR_INVALID_ARGUMENT);
    cr_assert_eq(ng_grid_store_line(grid, 6, line), NG_ERR_INVALID_ARGUMENT);
    cr_assert_eq(ng_grid_load_line(grid, UINT32_MAX, line), NG_ERR_INVALID_ARGUMENT);

    cr_assert_eq(line[0], NG_CELL_EMPTY, "buffer untouched");
    ng_grid_destroy(grid);
}

Test(grid_line, nullptr_arguments_are_invalid)
{
    ng_grid *grid = make_grid(2, 2);
    ng_cell line[2] = {};

    cr_assert_eq(ng_grid_load_line(nullptr, 0, line), NG_ERR_INVALID_ARGUMENT);
    cr_assert_eq(ng_grid_load_line(grid, 0, nullptr), NG_ERR_INVALID_ARGUMENT);
    cr_assert_eq(ng_grid_store_line(nullptr, 0, line), NG_ERR_INVALID_ARGUMENT);
    cr_assert_eq(ng_grid_store_line(grid, 0, nullptr), NG_ERR_INVALID_ARGUMENT);
    ng_grid_destroy(grid);
}

Test(grid_line, invalid_cell_value_is_rejected_before_any_write)
{
    ng_grid *grid = make_grid(3, 1);
    /* NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange) */
    const ng_cell bogus = (ng_cell)7;
    const ng_cell row[3] = {NG_CELL_FILLED, NG_CELL_FILLED, bogus};

    const ng_status status = ng_grid_store_line(grid, 0, row);

    cr_assert_eq(status, NG_ERR_INVALID_ARGUMENT);
    cr_assert_eq(ng_grid_get(grid, 0, 0), NG_CELL_UNKNOWN, "grid unchanged");
    ng_grid_destroy(grid);
}

Test(grid_line, full_column_of_a_tall_grid_round_trips)
{
    constexpr ng_dim height = 100'000;
    ng_grid *grid = make_grid(2, height);
    static ng_cell column[height];
    static ng_cell back[height];
    for (ng_dim r = 0; r < height; ++r) {
        column[r] = pattern(r, 1);
    }

    cr_assert_eq(ng_grid_store_line(grid, height + 1u, column), NG_OK);
    cr_assert_eq(ng_grid_load_line(grid, height + 1u, back), NG_OK);

    cr_assert_arr_eq(back, column, sizeof column);
    cr_assert_eq(ng_grid_get(grid, height - 1u, 0), NG_CELL_UNKNOWN, "column 0 untouched");
    ng_grid_destroy(grid);
}
