#include <criterion/criterion.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "nonogram/types.h"

#include "ng_test_fixture.h"

TestSuite(types_width, .init = ng_test_setup, .fini = ng_test_teardown);
TestSuite(types_limits, .init = ng_test_setup, .fini = ng_test_teardown);
TestSuite(types_format, .init = ng_test_setup, .fini = ng_test_teardown);

Test(types_width, line_level_types_are_32_bits)
{
    cr_assert_eq(sizeof(ng_dim), sizeof(uint32_t));
    cr_assert_eq(sizeof(ng_line_id), sizeof(uint32_t));
    cr_assert_eq(sizeof(ng_run_count), sizeof(uint32_t));
}

Test(types_width, grid_level_types_are_64_bits)
{
    cr_assert_eq(sizeof(ng_cell_index), sizeof(uint64_t));
    cr_assert_eq(sizeof(ng_run_index), sizeof(uint64_t));
    cr_assert_eq(sizeof(ng_counter), sizeof(uint64_t));
}

Test(types_limits, dim_max_is_two_to_the_31_minus_one)
{
    cr_assert_eq(NG_DIM_MAX, (ng_dim)2'147'483'647u);
}

Test(types_limits, line_ids_of_the_largest_grid_fit)
{
    const uint64_t line_count = (uint64_t)NG_DIM_MAX + NG_DIM_MAX;

    cr_assert_leq(line_count, (uint64_t)UINT32_MAX);
}

Test(types_limits, cell_count_of_the_largest_grid_fits)
{
    const ng_cell_index cells = (ng_cell_index)NG_DIM_MAX * NG_DIM_MAX;

    cr_assert_gt(cells, (ng_cell_index)UINT32_MAX);
    cr_assert_eq(cells / NG_DIM_MAX, (ng_cell_index)NG_DIM_MAX);
}

Test(types_limits, run_count_max_fills_a_maximal_line)
{
    /* runs of length 1 separated by one empty cell: 2k - 1 cells */
    const uint64_t cells_needed = (2u * (uint64_t)NG_RUN_COUNT_MAX) - 1u;

    cr_assert_eq(cells_needed, (uint64_t)NG_DIM_MAX);
}

Test(types_format, macros_print_the_maximal_values)
{
    char text[64] = {};

    const int written = snprintf(text, sizeof text, "%" NG_PRI_DIM " %" NG_PRI_CELL_INDEX,
                                 NG_DIM_MAX, (ng_cell_index)UINT64_MAX);

    cr_assert_gt(written, 0);
    cr_assert_str_eq(text, "2147483647 18446744073709551615");
}

Test(types_format, every_macro_is_defined)
{
    const char *const formats[] = {NG_PRI_DIM,        NG_PRI_LINE_ID,   NG_PRI_RUN_COUNT,
                                   NG_PRI_CELL_INDEX, NG_PRI_RUN_INDEX, NG_PRI_COUNTER};

    for (size_t i = 0; i < sizeof formats / sizeof formats[0]; ++i) {
        cr_assert_gt(strlen(formats[i]), 0u);
    }
}
