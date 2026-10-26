#include "test_puzzles.h"

#include <criterion/criterion.h>
#include <stddef.h>
#include <stdint.h>

#include "nonogram/grid.h"
#include "nonogram/puzzle.h"
#include "nonogram/status.h"
#include "nonogram/types.h"

#include "ng_test_fixture.h"
#include "test_rng.h"

static constexpr uint64_t density_steps = 10;

ng_grid *ng_test_random_grid(ng_test_rng *rng, ng_dim max_side, bool with_unknown)
{
    const ng_dim width = (ng_dim)(1u + ng_test_rng_below(rng, max_side));
    const ng_dim height = (ng_dim)(1u + ng_test_rng_below(rng, max_side));
    const uint64_t density = ng_test_rng_below(rng, density_steps + 1u);
    ng_grid *grid = nullptr;
    cr_assert_eq(ng_grid_create(&ng_test_heap()->base, width, height, &grid), NG_OK);
    for (ng_dim r = 0; r < height; ++r) {
        for (ng_dim c = 0; c < width; ++c) {
            ng_cell cell =
                ng_test_rng_below(rng, density_steps) < density ? NG_CELL_FILLED : NG_CELL_EMPTY;
            if (with_unknown && ng_test_rng_below(rng, density_steps) == 0u) {
                cell = NG_CELL_UNKNOWN;
            }
            ng_grid_set(grid, r, c, cell);
        }
    }
    return grid;
}

static ng_cell cell_on_line(const ng_grid *grid, ng_line_id line, ng_dim i)
{
    const ng_dim height = ng_grid_height(grid);
    return line < height ? ng_grid_get(grid, line, i) : ng_grid_get(grid, i, line - height);
}

ng_puzzle *ng_test_puzzle_from_grid(const ng_grid *grid)
{
    const ng_allocator *a = &ng_test_heap()->base;
    const ng_dim width = ng_grid_width(grid);
    const ng_dim height = ng_grid_height(grid);
    const size_t line_count = (size_t)width + height;
    /* Every run covers at least one cell, counted once by its row and once by its column. */
    const size_t max_runs = 2u * (size_t)width * height;
    ng_run_index *offsets = a->alloc(a->ctx, (line_count + 1u) * sizeof *offsets, 8);
    ng_dim *runs = a->alloc(a->ctx, max_runs * sizeof *runs, 4);
    cr_assert(offsets != nullptr && runs != nullptr);
    ng_run_index count = 0;
    offsets[0] = 0;
    for (ng_line_id line = 0; line < line_count; ++line) {
        const ng_dim length = line < height ? width : height;
        ng_dim run = 0;
        for (ng_dim i = 0; i <= length; ++i) {
            if (i < length && cell_on_line(grid, line, i) == NG_CELL_FILLED) {
                ++run;
            } else if (run != 0u) {
                runs[count++] = run;
                run = 0;
            }
        }
        offsets[line + 1u] = count;
    }
    ng_puzzle *puzzle = nullptr;
    cr_assert_eq(ng_puzzle_create(a, width, height, runs, offsets, &puzzle), NG_OK);
    a->free(a->ctx, offsets, (line_count + 1u) * sizeof *offsets);
    a->free(a->ctx, runs, max_runs * sizeof *runs);
    return puzzle;
}

void ng_test_assert_same_clues(const ng_puzzle *a, const ng_puzzle *b)
{
    /* Enough for the grids of ng_test_random_grid: a line of n cells has at most (n + 1) / 2
     * runs. */
    constexpr ng_run_count max_runs = 64;
    cr_assert_eq(ng_puzzle_width(a), ng_puzzle_width(b));
    cr_assert_eq(ng_puzzle_height(a), ng_puzzle_height(b));
    cr_assert_eq(ng_puzzle_total_runs(a), ng_puzzle_total_runs(b));
    for (ng_line_id line = 0; line < ng_puzzle_line_count(a); ++line) {
        ng_dim runs_a[max_runs] = {};
        ng_dim runs_b[max_runs] = {};
        const ng_run_count count = ng_puzzle_line_run_count(a, line);
        cr_assert_eq(ng_puzzle_line_run_count(b, line), count, "line %u", line);
        cr_assert_eq(ng_puzzle_copy_line_runs(a, line, runs_a, max_runs), NG_OK);
        cr_assert_eq(ng_puzzle_copy_line_runs(b, line, runs_b, max_runs), NG_OK);
        cr_assert_arr_eq(runs_a, runs_b, count * sizeof(ng_dim), "line %u", line);
    }
}
