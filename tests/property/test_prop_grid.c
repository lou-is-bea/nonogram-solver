#include <criterion/criterion.h>
#include <inttypes.h>
#include <stdint.h>
#include <string.h>

#include "nonogram/grid.h"
#include "nonogram/status.h"
#include "nonogram/types.h"

#include "ng_test_fixture.h"
#include "test_rng.h"

/* P-M1 (TESTING.md §4.4): store_line then load_line gives back the stored line, for any row
 * or column of any grid, and the row and column views of a grid agree cell by cell. */
TestSuite(prop_grid, .init = ng_test_setup, .fini = ng_test_teardown, .timeout = 30);

static constexpr int case_count = 1'000;
static constexpr ng_dim max_side = 40;

static ng_cell random_cell(ng_test_rng *rng)
{
    static const ng_cell cells[] = {NG_CELL_UNKNOWN, NG_CELL_EMPTY, NG_CELL_FILLED};
    return cells[ng_test_rng_below(rng, 3)];
}

static ng_dim random_side(ng_test_rng *rng)
{
    return (ng_dim)(1u + ng_test_rng_below(rng, max_side));
}

static ng_grid *random_grid(ng_test_rng *rng)
{
    const ng_dim width = random_side(rng);
    const ng_dim height = random_side(rng);
    ng_grid *grid = nullptr;
    cr_assert_eq(ng_grid_create(&ng_test_heap()->base, width, height, &grid), NG_OK);
    for (ng_dim r = 0; r < height; ++r) {
        for (ng_dim c = 0; c < width; ++c) {
            ng_grid_set(grid, r, c, random_cell(rng));
        }
    }
    return grid;
}

static ng_dim line_length(const ng_grid *grid, ng_line_id line)
{
    return line < ng_grid_height(grid) ? ng_grid_width(grid) : ng_grid_height(grid);
}

Test(prop_grid, stored_line_loads_back_and_other_lines_are_untouched)
{
    const uint64_t seed = ng_test_seed();
    ng_test_rng rng;
    ng_test_rng_seed(&rng, seed);

    for (int k = 0; k < case_count; ++k) {
        ng_grid *grid = random_grid(&rng);
        ng_grid *before = nullptr;
        cr_assert_eq(ng_grid_create(&ng_test_heap()->base, ng_grid_width(grid),
                                    ng_grid_height(grid), &before),
                     NG_OK);
        cr_assert_eq(ng_grid_copy(before, grid), NG_OK);
        const ng_line_id line_count = ng_grid_height(grid) + ng_grid_width(grid);
        const ng_line_id line = (ng_line_id)ng_test_rng_below(&rng, line_count);
        const ng_dim length = line_length(grid, line);
        ng_cell stored[max_side] = {};
        ng_cell loaded[max_side] = {};
        for (ng_dim i = 0; i < length; ++i) {
            stored[i] = random_cell(&rng);
        }

        cr_assert_eq(ng_grid_store_line(grid, line, stored), NG_OK);
        cr_assert_eq(ng_grid_load_line(grid, line, loaded), NG_OK);

        cr_assert_arr_eq(loaded, stored, length, "case %d, line %u (seed %" PRIu64 ")", k, line,
                         seed);
        for (ng_line_id other = 0; other < line_count; ++other) {
            if (other == line) {
                continue;
            }
            /* A row and a column share one cell: compare only cells outside @p line. */
            ng_cell now[max_side] = {};
            ng_cell then[max_side] = {};
            cr_assert_eq(ng_grid_load_line(grid, other, now), NG_OK);
            cr_assert_eq(ng_grid_load_line(before, other, then), NG_OK);
            const bool crosses = (line < ng_grid_height(grid)) != (other < ng_grid_height(grid));
            for (ng_dim i = 0; i < line_length(grid, other); ++i) {
                const bool shared =
                    crosses &&
                    (other < ng_grid_height(grid) ? i == line - ng_grid_height(grid) : i == line);
                if (!shared) {
                    cr_assert_eq(now[i], then[i],
                                 "case %d, line %u touched line %u (seed %" PRIu64 ")", k, line,
                                 other, seed);
                }
            }
        }
        ng_grid_destroy(before);
        ng_grid_destroy(grid);
    }
}

Test(prop_grid, row_and_column_views_agree_with_cell_access)
{
    const uint64_t seed = ng_test_seed();
    ng_test_rng rng;
    ng_test_rng_seed(&rng, seed);

    for (int k = 0; k < case_count; ++k) {
        ng_grid *grid = random_grid(&rng);
        const ng_dim width = ng_grid_width(grid);
        const ng_dim height = ng_grid_height(grid);

        for (ng_line_id line = 0; line < height + width; ++line) {
            ng_cell loaded[max_side] = {};
            cr_assert_eq(ng_grid_load_line(grid, line, loaded), NG_OK);
            for (ng_dim i = 0; i < line_length(grid, line); ++i) {
                const ng_cell expected = line < height ? ng_grid_get(grid, line, i)
                                                       : ng_grid_get(grid, i, line - height);
                cr_assert_eq(loaded[i], expected, "case %d, line %u, cell %u (seed %" PRIu64 ")", k,
                             line, i, seed);
            }
        }
        ng_grid_destroy(grid);
    }
}
