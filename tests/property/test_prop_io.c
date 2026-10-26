#include <criterion/criterion.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "nonogram/grid.h"
#include "nonogram/io.h"
#include "nonogram/puzzle.h"
#include "nonogram/status.h"
#include "nonogram/stream.h"
#include "nonogram/types.h"

#include "ng_test_fixture.h"
#include "test_puzzles.h"
#include "test_rng.h"
#include "test_streams.h"

/* P-IO1, P-IO2 and P-IO3 (TESTING.md §4.4). */
TestSuite(prop_io, .init = ng_test_setup, .fini = ng_test_teardown, .timeout = 60);

static constexpr int case_count = 1'000;
static constexpr ng_dim max_side = 30;
/* P-IO3 block sizes: one byte, an odd size, and the whole reader block. */
static const size_t chunks[] = {1, 7, 65'536};

static ng_buffer *new_buffer(ng_writer *writer)
{
    ng_buffer *buffer = nullptr;
    cr_assert_eq(ng_buffer_create(&ng_test_heap()->base, &buffer), NG_OK);
    *writer = ng_writer_from_buffer(buffer);
    return buffer;
}

static ng_puzzle *read_non_chunked(const ng_buffer *text, size_t chunk)
{
    ng_test_source source;
    const ng_reader r =
        ng_test_chunked_reader(&source, ng_buffer_data(text), ng_buffer_size(text), chunk);
    ng_puzzle *puzzle = nullptr;
    ng_parse_error error = {};
    cr_assert_eq(ng_io_read_non(&ng_test_heap()->base, &r, &puzzle, nullptr, &error), NG_OK,
                 "chunk %zu, %" PRIu64 ":%" PRIu64 ": %s", chunk, error.line, error.column,
                 error.message);
    return puzzle;
}

static ng_grid *read_sol_chunked(const ng_buffer *text, size_t chunk)
{
    ng_test_source source;
    const ng_reader r =
        ng_test_chunked_reader(&source, ng_buffer_data(text), ng_buffer_size(text), chunk);
    ng_grid *grid = nullptr;
    ng_parse_error error = {};
    cr_assert_eq(ng_io_read_sol(&ng_test_heap()->base, &r, &grid, &error), NG_OK, "chunk %zu: %s",
                 chunk, error.message);
    return grid;
}

static void assert_same_grid(const ng_grid *a, const ng_grid *b, int k, uint64_t seed)
{
    cr_assert_eq(ng_grid_width(a), ng_grid_width(b));
    cr_assert_eq(ng_grid_height(a), ng_grid_height(b));
    for (ng_dim r = 0; r < ng_grid_height(a); ++r) {
        for (ng_dim c = 0; c < ng_grid_width(a); ++c) {
            cr_assert_eq(ng_grid_get(a, r, c), ng_grid_get(b, r, c),
                         "case %d, cell (%u, %u) (seed %" PRIu64 ")", k, r, c, seed);
        }
    }
}

Test(prop_io, sol_round_trip_preserves_every_cell_for_any_block_size)
{
    const uint64_t seed = ng_test_seed();
    ng_test_rng rng;
    ng_test_rng_seed(&rng, seed);

    for (int k = 0; k < case_count; ++k) {
        ng_grid *grid = ng_test_random_grid(&rng, max_side, true);
        ng_writer w;
        ng_buffer *text = new_buffer(&w);
        cr_assert_eq(ng_io_write_sol(grid, &w), NG_OK);

        for (size_t i = 0; i < sizeof chunks / sizeof chunks[0]; ++i) {
            ng_grid *back = read_sol_chunked(text, chunks[i]);
            assert_same_grid(grid, back, k, seed);
            ng_grid_destroy(back);
        }
        ng_buffer_destroy(text);
        ng_grid_destroy(grid);
    }
}

Test(prop_io, non_round_trip_preserves_the_clues_for_any_block_size)
{
    const uint64_t seed = ng_test_seed();
    ng_test_rng rng;
    ng_test_rng_seed(&rng, seed);

    for (int k = 0; k < case_count; ++k) {
        ng_grid *grid = ng_test_random_grid(&rng, max_side, false);
        ng_puzzle *puzzle = ng_test_puzzle_from_grid(grid);
        ng_writer w;
        ng_buffer *text = new_buffer(&w);
        cr_assert_eq(ng_io_write_non(puzzle, nullptr, &w), NG_OK);

        for (size_t i = 0; i < sizeof chunks / sizeof chunks[0]; ++i) {
            ng_puzzle *back = read_non_chunked(text, chunks[i]);
            ng_test_assert_same_clues(puzzle, back);
            ng_puzzle_destroy(back);
        }
        ng_buffer_destroy(text);
        ng_puzzle_destroy(puzzle);
        ng_grid_destroy(grid);
    }
}

Test(prop_io, written_non_is_a_fixed_point)
{
    const uint64_t seed = ng_test_seed();
    ng_test_rng rng;
    ng_test_rng_seed(&rng, seed);

    for (int k = 0; k < case_count; ++k) {
        ng_grid *grid = ng_test_random_grid(&rng, max_side, false);
        ng_puzzle *puzzle = ng_test_puzzle_from_grid(grid);
        ng_writer w1;
        ng_writer w2;
        ng_buffer *first = new_buffer(&w1);
        ng_buffer *second = new_buffer(&w2);
        cr_assert_eq(ng_io_write_non(puzzle, nullptr, &w1), NG_OK);
        ng_puzzle *back = read_non_chunked(first, SIZE_MAX);

        cr_assert_eq(ng_io_write_non(back, nullptr, &w2), NG_OK);

        cr_assert_str_eq(ng_buffer_data(first), ng_buffer_data(second),
                         "case %d (seed %" PRIu64 ")", k, seed);
        ng_puzzle_destroy(back);
        ng_buffer_destroy(first);
        ng_buffer_destroy(second);
        ng_puzzle_destroy(puzzle);
        ng_grid_destroy(grid);
    }
}
