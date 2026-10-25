#include <criterion/criterion.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "nonogram/grid.h"
#include "nonogram/puzzle.h"
#include "nonogram/render.h"
#include "nonogram/status.h"
#include "nonogram/stream.h"
#include "nonogram/types.h"

#include "ng_test_fixture.h"
#include "test_puzzles.h"
#include "test_streams.h"

TestSuite(render_text, .init = ng_test_setup, .fini = ng_test_teardown);
TestSuite(render_summary, .init = ng_test_setup, .fini = ng_test_teardown);

/* The "P" of docs/FILE_FORMATS.md. */
static const char *const p_rows[] = {"###..", "#.#..", "###..", "#....", "#...."};

/* Grid from rows of '#', '.' and '?'. */
static ng_grid *grid_of(const char *const *rows, ng_dim height)
{
    const ng_dim width = (ng_dim)strlen(rows[0]);
    ng_grid *grid = nullptr;
    cr_assert_eq(ng_grid_create(&ng_test_heap()->base, width, height, &grid), NG_OK);
    for (ng_dim r = 0; r < height; ++r) {
        for (ng_dim c = 0; c < width; ++c) {
            const char s = rows[r][c];
            ng_grid_set(grid, r, c,
                        s == '#' ? NG_CELL_FILLED : (s == '.' ? NG_CELL_EMPTY : NG_CELL_UNKNOWN));
        }
    }
    return grid;
}

typedef struct rendered {
    ng_buffer *buffer;
    ng_status status;
} rendered;

static rendered render(const ng_puzzle *puzzle, const ng_grid *grid,
                       const ng_render_options *options)
{
    rendered out = {};
    cr_assert_eq(ng_buffer_create(&ng_test_heap()->base, &out.buffer), NG_OK);
    const ng_writer w = ng_writer_from_buffer(out.buffer);
    out.status = ng_render_text(puzzle, grid, options, &w);
    return out;
}

static void assert_no_trailing_space(const char *text)
{
    for (const char *nl = strchr(text, '\n'); nl != nullptr; nl = strchr(nl + 1, '\n')) {
        cr_assert(nl == text || nl[-1] != ' ', "trailing space before offset %td in:\n%s",
                  nl - text, text);
    }
}

static void assert_renders(const ng_puzzle *puzzle, const ng_grid *grid,
                           const ng_render_options *options, const char *expected)
{
    rendered out = render(puzzle, grid, options);

    cr_assert_eq(out.status, NG_OK);
    cr_assert_str_eq(ng_buffer_data(out.buffer), expected, "got:\n%s\nexpected:\n%s",
                     ng_buffer_data(out.buffer), expected);
    assert_no_trailing_space(ng_buffer_data(out.buffer));
    ng_buffer_destroy(out.buffer);
}

typedef struct p_fixture {
    ng_grid *grid;
    ng_puzzle *puzzle;
} p_fixture;

static p_fixture make_p(void)
{
    p_fixture p = {.grid = grid_of(p_rows, 5)};
    p.puzzle = ng_test_puzzle_from_grid(p.grid);
    return p;
}

static void release_p(p_fixture *p)
{
    ng_puzzle_destroy(p->puzzle);
    ng_grid_destroy(p->grid);
}

/* --- render_text -------------------------------------------------------------------------- */

Test(render_text, ascii_with_clues_matches_the_roadmap_layout)
{
    p_fixture p = make_p();
    const ng_render_options options = {.show_clues = true};

    assert_renders(p.puzzle, p.grid, &options,
                   "        1\n"
                   "      5 1 3 0 0\n"
                   "    +-----------+\n"
                   "  3 | # # # . . |\n"
                   "1 1 | # . # . . |\n"
                   "  3 | # # # . . |\n"
                   "  1 | # . . . . |\n"
                   "  1 | # . . . . |\n"
                   "    +-----------+\n");
    release_p(&p);
}

Test(render_text, default_options_show_the_clues_in_ascii)
{
    p_fixture p = make_p();
    const ng_render_options options = {.show_clues = true};
    rendered explicit_options = render(p.puzzle, p.grid, &options);

    rendered defaults = render(p.puzzle, p.grid, nullptr);

    cr_assert_eq(defaults.status, NG_OK);
    cr_assert_str_eq(ng_buffer_data(defaults.buffer), ng_buffer_data(explicit_options.buffer));
    ng_buffer_destroy(defaults.buffer);
    ng_buffer_destroy(explicit_options.buffer);
    release_p(&p);
}

Test(render_text, without_clues_only_the_framed_grid_remains)
{
    p_fixture p = make_p();
    const ng_render_options options = {};

    assert_renders(p.puzzle, p.grid, &options,
                   "+-----------+\n"
                   "| # # # . . |\n"
                   "| # . # . . |\n"
                   "| # # # . . |\n"
                   "| # . . . . |\n"
                   "| # . . . . |\n"
                   "+-----------+\n");
    release_p(&p);
}

Test(render_text, without_grid_every_cell_is_unknown)
{
    p_fixture p = make_p();
    const ng_render_options options = {.show_clues = true};

    assert_renders(p.puzzle, nullptr, &options,
                   "        1\n"
                   "      5 1 3 0 0\n"
                   "    +-----------+\n"
                   "  3 | ? ? ? ? ? |\n"
                   "1 1 | ? ? ? ? ? |\n"
                   "  3 | ? ? ? ? ? |\n"
                   "  1 | ? ? ? ? ? |\n"
                   "  1 | ? ? ? ? ? |\n"
                   "    +-----------+\n");
    release_p(&p);
}

Test(render_text, partial_grid_shows_unknown_cells)
{
    static const char *const partial[] = {"#?.", "???"};
    ng_grid *grid = grid_of(partial, 2);
    ng_grid *full = grid_of((const char *const[]){"#..", "..#"}, 2);
    ng_puzzle *puzzle = ng_test_puzzle_from_grid(full);
    const ng_render_options options = {};

    assert_renders(puzzle, grid, &options,
                   "+-------+\n"
                   "| # ? . |\n"
                   "| ? ? ? |\n"
                   "+-------+\n");
    ng_puzzle_destroy(puzzle);
    ng_grid_destroy(full);
    ng_grid_destroy(grid);
}

Test(render_text, unicode_uses_blocks_dots_and_box_drawing)
{
    p_fixture p = make_p();
    const ng_render_options options = {.show_clues = true, .unicode = true};

    assert_renders(p.puzzle, p.grid, &options,
                   "        1\n"
                   "      5 1 3 0 0\n"
                   "    ┌───────────┐\n"
                   "  3 │ █ █ █ · · │\n"
                   "1 1 │ █ · █ · · │\n"
                   "  3 │ █ █ █ · · │\n"
                   "  1 │ █ · · · · │\n"
                   "  1 │ █ · · · · │\n"
                   "    └───────────┘\n");
    release_p(&p);
}

Test(render_text, multi_digit_column_clues_widen_every_column)
{
    /* 2 wide x 10 tall: column clues 10 and 1. */
    static const char *const rows[] = {"##", "#.", "#.", "#.", "#.", "#.", "#.", "#.", "#.", "#."};
    ng_grid *grid = grid_of(rows, 10);
    ng_puzzle *puzzle = ng_test_puzzle_from_grid(grid);
    const ng_render_options options = {.show_clues = true};

    assert_renders(puzzle, grid, &options,
                   "    10  1\n"
                   "  +-------+\n"
                   "2 |  #  # |\n"
                   "1 |  #  . |\n"
                   "1 |  #  . |\n"
                   "1 |  #  . |\n"
                   "1 |  #  . |\n"
                   "1 |  #  . |\n"
                   "1 |  #  . |\n"
                   "1 |  #  . |\n"
                   "1 |  #  . |\n"
                   "1 |  #  . |\n"
                   "  +-------+\n");
    ng_puzzle_destroy(puzzle);
    ng_grid_destroy(grid);
}

Test(render_text, multi_digit_row_clues_widen_the_left_margin)
{
    /* 12 wide x 2 tall: row clues "12" and "1 1 1". */
    static const char *const rows[] = {"############", "#.#.#......."};
    ng_grid *grid = grid_of(rows, 2);
    ng_puzzle *puzzle = ng_test_puzzle_from_grid(grid);
    const ng_render_options options = {.show_clues = true};

    assert_renders(puzzle, grid, &options,
                   "        2 1 2 1 2 1 1 1 1 1 1 1\n"
                   "      +-------------------------+\n"
                   "   12 | # # # # # # # # # # # # |\n"
                   "1 1 1 | # . # . # . . . . . . . |\n"
                   "      +-------------------------+\n");
    ng_puzzle_destroy(puzzle);
    ng_grid_destroy(grid);
}

Test(render_text, one_by_one_puzzles)
{
    ng_grid *filled = grid_of((const char *const[]){"#"}, 1);
    ng_grid *blank = grid_of((const char *const[]){"."}, 1);
    ng_puzzle *filled_puzzle = ng_test_puzzle_from_grid(filled);
    ng_puzzle *blank_puzzle = ng_test_puzzle_from_grid(blank);
    const ng_render_options options = {.show_clues = true};

    assert_renders(filled_puzzle, filled, &options, "    1\n  +---+\n1 | # |\n  +---+\n");
    assert_renders(blank_puzzle, blank, &options, "    0\n  +---+\n0 | . |\n  +---+\n");
    ng_puzzle_destroy(filled_puzzle);
    ng_puzzle_destroy(blank_puzzle);
    ng_grid_destroy(filled);
    ng_grid_destroy(blank);
}

Test(render_text, mismatched_grid_is_invalid_and_writes_nothing)
{
    p_fixture p = make_p();
    ng_grid *other = grid_of((const char *const[]){"##"}, 1);
    ng_test_sink sink;
    const ng_writer w = ng_test_failing_writer(&sink, SIZE_MAX);

    const ng_status status = ng_render_text(p.puzzle, other, nullptr, &w);

    cr_assert_eq(status, NG_ERR_INVALID_ARGUMENT);
    cr_assert_eq(sink.written, 0u);
    ng_grid_destroy(other);
    release_p(&p);
}

Test(render_text, nullptr_arguments_are_invalid)
{
    p_fixture p = make_p();
    ng_test_sink sink;
    const ng_writer w = ng_test_failing_writer(&sink, SIZE_MAX);

    cr_assert_eq(ng_render_text(nullptr, p.grid, nullptr, &w), NG_ERR_INVALID_ARGUMENT);
    cr_assert_eq(ng_render_text(p.puzzle, p.grid, nullptr, nullptr), NG_ERR_INVALID_ARGUMENT);
    release_p(&p);
}

Test(render_text, writer_failure_is_reported_at_every_point)
{
    p_fixture p = make_p();
    const ng_render_options options = {.show_clues = true, .unicode = true};
    rendered reference = render(p.puzzle, p.grid, &options);
    const size_t total = ng_buffer_size(reference.buffer);

    for (size_t limit = 0; limit <= total; ++limit) {
        ng_test_sink sink;
        const ng_writer w = ng_test_failing_writer(&sink, limit);

        const ng_status status = ng_render_text(p.puzzle, p.grid, &options, &w);

        cr_assert_eq(status, limit < total ? NG_ERR_IO : NG_OK, "limit %zu", limit);
    }
    ng_buffer_destroy(reference.buffer);
    release_p(&p);
}

/* --- render_summary ----------------------------------------------------------------------- */

static void assert_summary(const ng_puzzle *puzzle, const ng_grid *grid, const char *expected)
{
    ng_buffer *buffer = nullptr;
    cr_assert_eq(ng_buffer_create(&ng_test_heap()->base, &buffer), NG_OK);
    const ng_writer w = ng_writer_from_buffer(buffer);

    cr_assert_eq(ng_render_summary(puzzle, grid, &w), NG_OK);

    cr_assert_str_eq(ng_buffer_data(buffer), expected);
    ng_buffer_destroy(buffer);
}

Test(render_summary, counts_lines_runs_and_cells)
{
    p_fixture p = make_p();

    assert_summary(p.puzzle, p.grid,
                   "puzzle: 5 x 5, 10 lines, 10 runs, 10 filled cells\n"
                   "grid: 10 filled, 15 empty, 0 unknown\n");
    release_p(&p);
}

Test(render_summary, without_grid_only_the_puzzle_line)
{
    p_fixture p = make_p();

    assert_summary(p.puzzle, nullptr, "puzzle: 5 x 5, 10 lines, 10 runs, 10 filled cells\n");
    release_p(&p);
}

Test(render_summary, unknown_cells_are_counted)
{
    static const char *const partial[] = {"#?.", "???"};
    ng_grid *grid = grid_of(partial, 2);
    ng_puzzle *puzzle = ng_test_puzzle_from_grid(grid);

    assert_summary(puzzle, grid,
                   "puzzle: 3 x 2, 5 lines, 2 runs, 1 filled cells\n"
                   "grid: 1 filled, 1 empty, 4 unknown\n");
    ng_puzzle_destroy(puzzle);
    ng_grid_destroy(grid);
}

Test(render_summary, invalid_arguments_are_rejected)
{
    p_fixture p = make_p();
    ng_grid *other = grid_of((const char *const[]){"##"}, 1);
    ng_test_sink sink;
    const ng_writer w = ng_test_failing_writer(&sink, SIZE_MAX);

    cr_assert_eq(ng_render_summary(nullptr, nullptr, &w), NG_ERR_INVALID_ARGUMENT);
    cr_assert_eq(ng_render_summary(p.puzzle, nullptr, nullptr), NG_ERR_INVALID_ARGUMENT);
    cr_assert_eq(ng_render_summary(p.puzzle, other, &w), NG_ERR_INVALID_ARGUMENT);
    cr_assert_eq(sink.written, 0u);
    ng_grid_destroy(other);
    release_p(&p);
}

Test(render_summary, writer_failure_is_reported)
{
    p_fixture p = make_p();
    ng_test_sink sink;
    const ng_writer w = ng_test_failing_writer(&sink, 3);

    cr_assert_eq(ng_render_summary(p.puzzle, p.grid, &w), NG_ERR_IO);
    release_p(&p);
}
