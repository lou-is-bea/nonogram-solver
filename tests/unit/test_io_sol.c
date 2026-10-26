#include <criterion/criterion.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "nonogram/grid.h"
#include "nonogram/io.h"
#include "nonogram/status.h"
#include "nonogram/stream.h"
#include "nonogram/types.h"

#include "ng_test_fixture.h"
#include "test_allocator.h"
#include "test_streams.h"

TestSuite(io_read_sol, .init = ng_test_setup, .fini = ng_test_teardown);
TestSuite(io_write_sol, .init = ng_test_setup, .fini = ng_test_teardown);

/* Solution of the "P" of docs/FILE_FORMATS.md §2.1, with one unknown cell added. */
static const char p_sol[] = "###..\n"
                            "#.#..\n"
                            "###..\n"
                            "#...?\n"
                            "#....\n";

typedef struct sol_result {
    ng_status status;
    ng_grid *grid;
    ng_parse_error error;
} sol_result;

static sol_result read_chunked(const char *text, size_t size, size_t chunk)
{
    sol_result result = {};
    ng_test_source source;
    const ng_reader r = ng_test_chunked_reader(&source, text, size, chunk);
    result.status = ng_io_read_sol(&ng_test_heap()->base, &r, &result.grid, &result.error);
    return result;
}

static sol_result read_text(const char *text)
{
    return read_chunked(text, strlen(text), SIZE_MAX);
}

static ng_cell cell_of(char c)
{
    return c == '#' ? NG_CELL_FILLED : (c == '.' ? NG_CELL_EMPTY : NG_CELL_UNKNOWN);
}

static void assert_is_p(const ng_grid *grid)
{
    cr_assert_eq(ng_grid_width(grid), 5u);
    cr_assert_eq(ng_grid_height(grid), 5u);
    for (ng_dim r = 0; r < 5; ++r) {
        for (ng_dim c = 0; c < 5; ++c) {
            cr_assert_eq(ng_grid_get(grid, r, c), cell_of(p_sol[(r * 6u) + c]), "(%u, %u)", r, c);
        }
    }
}

static void assert_fails(const char *text, ng_status status, uint64_t line, uint64_t column,
                         const char *message)
{
    sol_result result = read_text(text);

    cr_assert_eq(result.status, status, "status %d (message: %s)", result.status,
                 result.error.message);
    cr_assert_null(result.grid);
    cr_assert_eq(result.error.line, line, "line (message: %s)", result.error.message);
    cr_assert_eq(result.error.column, column, "column (message: %s)", result.error.message);
    cr_assert_not_null(strstr(result.error.message, message), "message '%s' lacks '%s'",
                       result.error.message, message);
}

/* --- io_read_sol -------------------------------------------------------------------------- */

Test(io_read_sol, reads_every_cell_state)
{
    sol_result result = read_text(p_sol);

    cr_assert_eq(result.status, NG_OK, "%s", result.error.message);
    assert_is_p(result.grid);
    cr_assert_eq(result.error.line, 0u);
    ng_grid_destroy(result.grid);
}

Test(io_read_sol, block_size_does_not_change_the_result)
{
    const size_t chunks[] = {1, 2, 7, 64};

    for (size_t i = 0; i < sizeof chunks / sizeof chunks[0]; ++i) {
        sol_result result = read_chunked(p_sol, strlen(p_sol), chunks[i]);

        cr_assert_eq(result.status, NG_OK, "chunk %zu: %s", chunks[i], result.error.message);
        assert_is_p(result.grid);
        ng_grid_destroy(result.grid);
    }
}

Test(io_read_sol, final_newline_is_optional_and_crlf_accepted)
{
    sol_result bare = read_text("#.\n.#");
    sol_result crlf = read_text("#.\r\n.#\r\n");

    cr_assert_eq(bare.status, NG_OK, "%s", bare.error.message);
    cr_assert_eq(crlf.status, NG_OK, "%s", crlf.error.message);
    cr_assert_eq(ng_grid_height(bare.grid), 2u);
    cr_assert_eq(ng_grid_get(crlf.grid, 1, 1), NG_CELL_FILLED);
    ng_grid_destroy(bare.grid);
    ng_grid_destroy(crlf.grid);
}

Test(io_read_sol, single_cell_and_elongated_grids)
{
    sol_result one = read_text("#\n");
    sol_result column = read_text("#\n.\n#\n");

    cr_assert_eq(one.status, NG_OK);
    cr_assert_eq(ng_grid_width(one.grid), 1u);
    cr_assert_eq(column.status, NG_OK);
    cr_assert_eq(ng_grid_width(column.grid), 1u);
    cr_assert_eq(ng_grid_height(column.grid), 3u);
    ng_grid_destroy(one.grid);
    ng_grid_destroy(column.grid);
}

Test(io_read_sol, empty_input_is_rejected)
{
    assert_fails("", NG_ERR_PARSE, 1, 1, "empty grid");
    assert_fails("\n", NG_ERR_PARSE, 1, 1, "empty line");
}

Test(io_read_sol, unknown_character_is_rejected_at_its_position)
{
    assert_fails("##.\n#x.\n", NG_ERR_PARSE, 2, 2, "invalid character 'x'");
    assert_fails("## \n", NG_ERR_PARSE, 1, 3, "invalid character ' '");
}

Test(io_read_sol, lines_of_different_lengths_are_rejected)
{
    assert_fails("###\n##\n", NG_ERR_PARSE, 2, 3, "line 2 has 2 cells, expected 3");
    assert_fails("##\n###\n", NG_ERR_PARSE, 2, 3, "line 2 has more than 2 cells");
    assert_fails("##\n##\n\n", NG_ERR_PARSE, 3, 1, "empty line");
}

Test(io_read_sol, lone_carriage_return_is_rejected)
{
    assert_fails("#\r#\n", NG_ERR_PARSE, 1, 2, "carriage return");
}

Test(io_read_sol, every_allocation_failure_is_clean)
{
    ng_test_allocator *ta = ng_test_heap();
    for (ng_counter n = 0;; ++n) {
        ng_test_allocator_fail_at(ta, n);

        sol_result result = read_text(p_sol);

        if (result.status == NG_OK) {
            ng_grid_destroy(result.grid);
            break;
        }
        cr_assert_eq(result.status, NG_ERR_OUT_OF_MEMORY);
        cr_assert_null(result.grid);
        cr_assert_eq(ta->live_bytes, 0u);
    }
}

Test(io_read_sol, reader_failure_is_an_io_error)
{
    ng_test_source source;
    const ng_reader r = ng_test_failing_reader(&source, p_sol, strlen(p_sol), 8);
    ng_grid *grid = nullptr;
    ng_parse_error error = {};

    const ng_status status = ng_io_read_sol(&ng_test_heap()->base, &r, &grid, &error);

    cr_assert_eq(status, NG_ERR_IO);
    cr_assert_null(grid);
    cr_assert_eq(error.line, 2u);
    cr_assert_eq(error.column, 3u);
}

Test(io_read_sol, nullptr_arguments_are_invalid)
{
    ng_memory_source source;
    const ng_reader r = ng_reader_from_memory(&source, p_sol, strlen(p_sol));
    ng_grid *grid = (ng_grid *)&grid;

    cr_assert_eq(ng_io_read_sol(nullptr, &r, &grid, nullptr), NG_ERR_INVALID_ARGUMENT);
    cr_assert_null(grid);
    cr_assert_eq(ng_io_read_sol(&ng_test_heap()->base, nullptr, &grid, nullptr),
                 NG_ERR_INVALID_ARGUMENT);
    cr_assert_eq(ng_io_read_sol(&ng_test_heap()->base, &r, nullptr, nullptr),
                 NG_ERR_INVALID_ARGUMENT);
}

/* --- io_write_sol ------------------------------------------------------------------------- */

Test(io_write_sol, writes_one_line_per_row_with_a_final_newline)
{
    sol_result result = read_text(p_sol);
    cr_assert_eq(result.status, NG_OK);
    ng_buffer *buffer = nullptr;
    cr_assert_eq(ng_buffer_create(&ng_test_heap()->base, &buffer), NG_OK);
    const ng_writer w = ng_writer_from_buffer(buffer);

    const ng_status status = ng_io_write_sol(result.grid, &w);

    cr_assert_eq(status, NG_OK);
    cr_assert_str_eq(ng_buffer_data(buffer), p_sol);
    ng_buffer_destroy(buffer);
    ng_grid_destroy(result.grid);
}

Test(io_write_sol, rows_wider_than_the_staging_buffer_are_complete)
{
    constexpr ng_dim width = 10'000;
    ng_grid *grid = nullptr;
    cr_assert_eq(ng_grid_create(&ng_test_heap()->base, width, 2, &grid), NG_OK);
    ng_grid_fill(grid, NG_CELL_EMPTY);
    ng_grid_set(grid, 1, width - 1u, NG_CELL_FILLED);
    ng_buffer *buffer = nullptr;
    cr_assert_eq(ng_buffer_create(&ng_test_heap()->base, &buffer), NG_OK);
    const ng_writer w = ng_writer_from_buffer(buffer);

    cr_assert_eq(ng_io_write_sol(grid, &w), NG_OK);

    const char *text = ng_buffer_data(buffer);
    cr_assert_eq(ng_buffer_size(buffer), (size_t)2 * (width + 1u));
    cr_assert_eq(text[width], '\n');
    cr_assert_eq(text[(size_t)2 * width], '#');
    ng_buffer_destroy(buffer);
    ng_grid_destroy(grid);
}

Test(io_write_sol, writer_failure_is_reported_at_every_point)
{
    sol_result result = read_text(p_sol);
    cr_assert_eq(result.status, NG_OK);
    const size_t total = sizeof p_sol - 1u;

    for (size_t limit = 0; limit <= total; ++limit) {
        ng_test_sink sink;
        const ng_writer w = ng_test_failing_writer(&sink, limit);

        const ng_status status = ng_io_write_sol(result.grid, &w);

        cr_assert_eq(status, limit < total ? NG_ERR_IO : NG_OK, "limit %zu", limit);
    }
    ng_grid_destroy(result.grid);
}

Test(io_write_sol, nullptr_arguments_are_invalid)
{
    ng_grid *grid = nullptr;
    cr_assert_eq(ng_grid_create(&ng_test_heap()->base, 1, 1, &grid), NG_OK);
    ng_test_sink sink;
    const ng_writer w = ng_test_failing_writer(&sink, SIZE_MAX);

    cr_assert_eq(ng_io_write_sol(nullptr, &w), NG_ERR_INVALID_ARGUMENT);
    cr_assert_eq(ng_io_write_sol(grid, nullptr), NG_ERR_INVALID_ARGUMENT);
    ng_grid_destroy(grid);
}
