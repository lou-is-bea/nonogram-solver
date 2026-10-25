#include <criterion/criterion.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "nonogram/io.h"
#include "nonogram/puzzle.h"
#include "nonogram/status.h"
#include "nonogram/stream.h"
#include "nonogram/types.h"

#include "ng_test_fixture.h"
#include "test_allocator.h"
#include "test_streams.h"

TestSuite(io_read_non, .init = ng_test_setup, .fini = ng_test_teardown);
TestSuite(io_read_non_errors, .init = ng_test_setup, .fini = ng_test_teardown);
TestSuite(io_write_non, .init = ng_test_setup, .fini = ng_test_teardown);

/* The example of docs/FILE_FORMATS.md §1.1. */
static const char p_text[] = "# Exemple : un « P » minuscule\n"
                             "title \"Lettre P\"\n"
                             "author \"nonogram-solver\"\n"
                             "width 5\n"
                             "height 5\n"
                             "\n"
                             "rows\n"
                             "3\n"
                             "1,1\n"
                             "3\n"
                             "1\n"
                             "1\n"
                             "\n"
                             "columns\n"
                             "5\n"
                             "1,1\n"
                             "3\n"
                             "0\n"
                             "0\n";

/* Clues of p_text, line by line; 0 terminates each clue. */
static const ng_dim p_clues[10][3] = {{3}, {1, 1}, {3}, {1}, {1}, {5}, {1, 1}, {3}, {}, {}};

typedef struct read_result {
    ng_status status;
    ng_puzzle *puzzle;
    ng_non_metadata *metadata;
    ng_parse_error error;
} read_result;

static read_result read_chunked(const char *text, size_t size, size_t chunk)
{
    read_result result = {};
    ng_test_source source;
    const ng_reader r = ng_test_chunked_reader(&source, text, size, chunk);
    result.status =
        ng_io_read_non(&ng_test_heap()->base, &r, &result.puzzle, &result.metadata, &result.error);
    return result;
}

static read_result read_text(const char *text)
{
    return read_chunked(text, strlen(text), SIZE_MAX);
}

static void release(read_result *result)
{
    ng_puzzle_destroy(result->puzzle);
    ng_non_metadata_destroy(result->metadata);
}

static void assert_clue(const ng_puzzle *puzzle, ng_line_id line, const ng_dim *expected)
{
    ng_run_count count = 0;
    while (count < 3 && expected[count] != 0u) {
        ++count;
    }
    ng_dim runs[3] = {};
    cr_assert_eq(ng_puzzle_line_run_count(puzzle, line), count, "line %u", line);
    cr_assert_eq(ng_puzzle_copy_line_runs(puzzle, line, runs, 3), NG_OK);
    cr_assert_arr_eq(runs, expected, count * sizeof(ng_dim), "line %u", line);
}

static void assert_is_p(const ng_puzzle *puzzle)
{
    cr_assert_eq(ng_puzzle_width(puzzle), 5u);
    cr_assert_eq(ng_puzzle_height(puzzle), 5u);
    for (ng_line_id line = 0; line < 10; ++line) {
        assert_clue(puzzle, line, p_clues[line]);
    }
}

/* Reads @p text and checks the failure: status, position and message fragment. */
static void assert_fails(const char *text, ng_status status, uint64_t line, uint64_t column,
                         const char *message)
{
    read_result result = read_text(text);

    cr_assert_eq(result.status, status, "status %d for: %s (message: %s)", result.status, text,
                 result.error.message);
    cr_assert_null(result.puzzle);
    cr_assert_null(result.metadata);
    cr_assert_eq(result.error.line, line, "line for: %s (message: %s)", text, result.error.message);
    cr_assert_eq(result.error.column, column, "column for: %s (message: %s)", text,
                 result.error.message);
    cr_assert_not_null(strstr(result.error.message, message), "message '%s' lacks '%s'",
                       result.error.message, message);
}

/* --- io_read_non -------------------------------------------------------------------------- */

Test(io_read_non, reads_the_reference_example)
{
    read_result result = read_text(p_text);

    cr_assert_eq(result.status, NG_OK, "%s", result.error.message);
    assert_is_p(result.puzzle);
    cr_assert_str_eq(ng_non_metadata_get(result.metadata, NG_NON_TITLE), "Lettre P");
    cr_assert_str_eq(ng_non_metadata_get(result.metadata, NG_NON_AUTHOR), "nonogram-solver");
    cr_assert_null(ng_non_metadata_get(result.metadata, NG_NON_GOAL));
    cr_assert_eq(result.error.line, 0u);
    cr_assert_str_eq(result.error.message, "");
    release(&result);
}

Test(io_read_non, block_size_does_not_change_the_result)
{
    const size_t chunks[] = {1, 2, 7, 64};

    for (size_t i = 0; i < sizeof chunks / sizeof chunks[0]; ++i) {
        read_result result = read_chunked(p_text, strlen(p_text), chunks[i]);

        cr_assert_eq(result.status, NG_OK, "chunk %zu: %s", chunks[i], result.error.message);
        assert_is_p(result.puzzle);
        release(&result);
    }
}

Test(io_read_non, crlf_line_endings_are_accepted)
{
    const char *text = "width 2\r\nheight 1\r\nrows\r\n2\r\ncolumns\r\n1\r\n1\r\n";
    read_result result = read_text(text);

    cr_assert_eq(result.status, NG_OK, "%s", result.error.message);
    cr_assert_eq(ng_puzzle_width(result.puzzle), 2u);
    release(&result);
}

Test(io_read_non, final_newline_is_optional)
{
    read_result result = read_text("width 1\nheight 1\nrows\n1\ncolumns\n1");

    cr_assert_eq(result.status, NG_OK, "%s", result.error.message);
    release(&result);
}

Test(io_read_non, separators_may_be_commas_or_spaces_with_padding)
{
    const char *text = "width 9\nheight 1\nrows\n  1 , 1  1,1 \ncolumns\n"
                       "1\n0\n1\n0\n1\n0\n1\n0\n0\n";
    read_result result = read_text(text);
    const ng_dim expected[] = {1, 1, 1};

    cr_assert_eq(result.status, NG_OK, "%s", result.error.message);
    cr_assert_eq(ng_puzzle_line_run_count(result.puzzle, 0), 4u);
    ng_dim runs[4] = {};
    cr_assert_eq(ng_puzzle_copy_line_runs(result.puzzle, 0, runs, 4), NG_OK);
    cr_assert_arr_eq(runs, expected, sizeof expected);
    release(&result);
}

Test(io_read_non, comments_inside_a_section_are_not_clues)
{
    const char *text = "width 1\nheight 2\nrows\n# first\n1\n#second\n0\ncolumns\n1\n";
    read_result result = read_text(text);

    cr_assert_eq(result.status, NG_OK, "%s", result.error.message);
    cr_assert_eq(ng_puzzle_line_run_count(result.puzzle, 0), 1u);
    cr_assert_eq(ng_puzzle_line_run_count(result.puzzle, 1), 0u);
    release(&result);
}

Test(io_read_non, directives_after_the_sections_are_kept)
{
    const char *text = "width 1\nheight 1\nrows\n1\ncolumns\n1\n\ngoal \"1\"\nlicense \"MIT\"\n";
    read_result result = read_text(text);

    cr_assert_eq(result.status, NG_OK, "%s", result.error.message);
    cr_assert_str_eq(ng_non_metadata_get(result.metadata, NG_NON_GOAL), "1");
    cr_assert_str_eq(ng_non_metadata_get(result.metadata, NG_NON_LICENSE), "MIT");
    release(&result);
}

Test(io_read_non, every_text_directive_is_kept_and_empty_differs_from_absent)
{
    const char *text = "title \"\"\nauthor \"A\"\ncopyright \"C\"\nlicense \"L\"\n"
                       "catalogue \"K\"\nwidth 1\nheight 1\nrows\n0\ncolumns\n0\n";
    read_result result = read_text(text);

    cr_assert_eq(result.status, NG_OK, "%s", result.error.message);
    cr_assert_str_eq(ng_non_metadata_get(result.metadata, NG_NON_TITLE), "");
    cr_assert_str_eq(ng_non_metadata_get(result.metadata, NG_NON_AUTHOR), "A");
    cr_assert_str_eq(ng_non_metadata_get(result.metadata, NG_NON_COPYRIGHT), "C");
    cr_assert_str_eq(ng_non_metadata_get(result.metadata, NG_NON_LICENSE), "L");
    cr_assert_str_eq(ng_non_metadata_get(result.metadata, NG_NON_CATALOGUE), "K");
    cr_assert_null(ng_non_metadata_get(result.metadata, NG_NON_GOAL));
    release(&result);
}

Test(io_read_non, utf8_text_values_are_kept_byte_for_byte)
{
    read_result result = read_text("title \"Lettre « P »\"\nwidth 1\nheight 1\nrows\n0\n"
                                   "columns\n0\n");

    cr_assert_eq(result.status, NG_OK, "%s", result.error.message);
    cr_assert_str_eq(ng_non_metadata_get(result.metadata, NG_NON_TITLE), "Lettre « P »");
    release(&result);
}

Test(io_read_non, metadata_may_be_discarded)
{
    ng_memory_source source;
    const ng_reader r = ng_reader_from_memory(&source, p_text, strlen(p_text));
    ng_puzzle *puzzle = nullptr;

    const ng_status status = ng_io_read_non(&ng_test_heap()->base, &r, &puzzle, nullptr, nullptr);

    cr_assert_eq(status, NG_OK);
    assert_is_p(puzzle);
    ng_puzzle_destroy(puzzle);
}

Test(io_read_non, clue_lines_longer_than_a_block_are_read)
{
    /* One row of 100 000 runs of 1: about 200 KB on a single line, three times a block. */
    constexpr ng_dim runs = 100'000;
    constexpr ng_dim width = (2u * runs) - 1u;
    ng_buffer *text = nullptr;
    cr_assert_eq(ng_buffer_create(&ng_test_heap()->base, &text), NG_OK);
    const ng_writer w = ng_writer_from_buffer(text);
    char header[64] = {};
    const int length = snprintf(header, sizeof header, "width %u\nheight 1\nrows\n1", width);
    cr_assert_eq(w.write(w.ctx, header, (size_t)length), NG_OK);
    for (ng_dim i = 1; i < runs; ++i) {
        cr_assert_eq(w.write(w.ctx, ",1", 2), NG_OK);
    }
    cr_assert_eq(w.write(w.ctx, "\ncolumns\n", 9), NG_OK);
    for (ng_dim c = 0; c < width; ++c) {
        cr_assert_eq(w.write(w.ctx, c % 2u == 0u ? "1\n" : "0\n", 2), NG_OK);
    }

    read_result result = read_text(ng_buffer_data(text));

    cr_assert_eq(result.status, NG_OK, "%s", result.error.message);
    cr_assert_eq(ng_puzzle_line_run_count(result.puzzle, 0), runs);
    cr_assert_eq(ng_puzzle_total_runs(result.puzzle), (ng_run_index)2 * runs);
    release(&result);
    ng_buffer_destroy(text);
}

Test(io_read_non, every_allocation_failure_is_clean)
{
    ng_test_allocator *ta = ng_test_heap();
    for (ng_counter n = 0;; ++n) {
        ng_test_allocator_fail_at(ta, n);

        read_result result = read_text(p_text);

        if (result.status == NG_OK) {
            release(&result);
            break;
        }
        cr_assert_eq(result.status, NG_ERR_OUT_OF_MEMORY, "allocation %llu: %s",
                     (unsigned long long)n, result.error.message);
        cr_assert_null(result.puzzle);
        cr_assert_null(result.metadata);
        cr_assert_eq(ta->live_bytes, 0u);
    }
}

Test(io_read_non, reader_failure_is_an_io_error_at_its_position)
{
    ng_test_source source;
    const ng_reader r = ng_test_failing_reader(&source, p_text, strlen(p_text), 60);
    ng_puzzle *puzzle = nullptr;
    ng_parse_error error = {};

    const ng_status status = ng_io_read_non(&ng_test_heap()->base, &r, &puzzle, nullptr, &error);

    cr_assert_eq(status, NG_ERR_IO);
    cr_assert_null(puzzle);
    cr_assert_gt(error.line, 1u);
    cr_assert_neq(error.message[0], '\0');
}

Test(io_read_non, huge_dimensions_fail_cleanly_on_allocation)
{
    ng_test_allocator *ta = ng_test_heap();
    ng_test_allocator_cap(ta, (size_t)1 << 20);

    read_result result = read_text("width 2147483647\nheight 2147483647\nrows\n");

    cr_assert_eq(result.status, NG_ERR_OUT_OF_MEMORY, "%s", result.error.message);
    cr_assert_null(result.puzzle);
    /* (2^31 - 1) * 2 + 1 line offsets of 8 bytes each. */
    cr_assert_geq(ta->last_request_size, (size_t)4'294'967'295u * 8u);
}

Test(io_read_non, nullptr_arguments_are_invalid)
{
    ng_memory_source source;
    const ng_reader r = ng_reader_from_memory(&source, p_text, strlen(p_text));
    ng_puzzle *puzzle = (ng_puzzle *)&puzzle;

    cr_assert_eq(ng_io_read_non(nullptr, &r, &puzzle, nullptr, nullptr), NG_ERR_INVALID_ARGUMENT);
    cr_assert_null(puzzle);
    cr_assert_eq(ng_io_read_non(&ng_test_heap()->base, nullptr, &puzzle, nullptr, nullptr),
                 NG_ERR_INVALID_ARGUMENT);
    cr_assert_eq(ng_io_read_non(&ng_test_heap()->base, &r, nullptr, nullptr, nullptr),
                 NG_ERR_INVALID_ARGUMENT);
}

/* --- io_read_non_errors: FILE_FORMATS.md §1.4 and the strict rules of §1.3 ---------------- */

Test(io_read_non_errors, missing_width)
{
    assert_fails("height 1\nrows\n1\ncolumns\n1\n", NG_ERR_PARSE, 2, 1,
                 "missing 'width' directive");
}

Test(io_read_non_errors, missing_height)
{
    assert_fails("width 1\n", NG_ERR_PARSE, 2, 1, "missing 'height' directive");
}

Test(io_read_non_errors, missing_rows_section)
{
    assert_fails("width 1\nheight 1\n", NG_ERR_PARSE, 3, 1, "missing 'rows' section");
}

Test(io_read_non_errors, missing_columns_section)
{
    assert_fails("width 1\nheight 1\nrows\n1\n", NG_ERR_PARSE, 5, 1, "missing 'columns' section");
}

Test(io_read_non_errors, zero_width_is_out_of_range)
{
    assert_fails("width 0\n", NG_ERR_PARSE, 1, 7, "width out of range [1, 2147483647]: 0");
}

Test(io_read_non_errors, width_above_dim_max_is_out_of_range)
{
    assert_fails("width 2147483648\n", NG_ERR_PARSE, 1, 7,
                 "width out of range [1, 2147483647]: 2147483648");
    assert_fails("height 99999999999999999999999\n", NG_ERR_PARSE, 1, 8,
                 "height out of range [1, 2147483647]");
}

Test(io_read_non_errors, non_numeric_dimension)
{
    assert_fails("width five\n", NG_ERR_PARSE, 1, 7, "invalid value for 'width': 'five'");
    assert_fails("width 05\n", NG_ERR_PARSE, 1, 7, "invalid value for 'width': '05'");
    assert_fails("width\n", NG_ERR_PARSE, 1, 6, "invalid value for 'width'");
}

Test(io_read_non_errors, duplicate_directive)
{
    assert_fails("width 1\nwidth 1\n", NG_ERR_PARSE, 2, 1, "duplicate directive 'width'");
    assert_fails("title \"a\"\ntitle \"b\"\n", NG_ERR_PARSE, 2, 1, "duplicate directive 'title'");
}

Test(io_read_non_errors, unknown_directive)
{
    assert_fails("colour 1\n", NG_ERR_PARSE, 1, 1, "unknown directive 'colour'");
    assert_fails("Width 1\n", NG_ERR_PARSE, 1, 1, "unknown directive 'Width'");
}

Test(io_read_non_errors, too_few_row_clues)
{
    assert_fails("width 5\nheight 5\nrows\n3\n1,1\n3\n1\ncolumns\n", NG_ERR_PARSE, 8, 1,
                 "expected 5 row clues, got 4");
}

Test(io_read_non_errors, too_many_row_clues)
{
    assert_fails("width 1\nheight 1\nrows\n1\n0\ncolumns\n1\n", NG_ERR_PARSE, 5, 1,
                 "expected 1 row clues, got more");
}

Test(io_read_non_errors, too_few_column_clues)
{
    assert_fails("width 3\nheight 1\nrows\n1,1\ncolumns\n1\n0\n", NG_ERR_PARSE, 8, 1,
                 "expected 3 column clues, got 2");
}

Test(io_read_non_errors, extra_clue_after_the_columns)
{
    assert_fails("width 1\nheight 1\nrows\n1\ncolumns\n1\n1\n", NG_ERR_PARSE, 7, 1,
                 "expected 1 column clues, got more");
}

Test(io_read_non_errors, invalid_clue_reports_its_text_and_position)
{
    const char *header = "width 5\nheight 1\nrows\n";
    char text[128] = {};
    const struct {
        const char *clue;
        const char *message;
        uint64_t column;
    } cases[] = {
        {"1,1a", "invalid clue '1a' at position 2", 3},
        {"01", "invalid clue '01' at position 1", 1},
        {"0,1", "invalid clue '0' at position 1", 1},
        {"1,0", "invalid clue '0' at position 2", 3},
        {"1,,1", "invalid clue ',' at position 2", 3},
        {"1,", "missing clue at position 2", 3},
        {"-1", "invalid clue '-1' at position 1", 1},
        {"2147483648", "invalid clue '2147483648' at position 1", 1},
        {"1\t1", "invalid clue '1\\t1' at position 1", 1},
    };

    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; ++i) {
        const int length =
            snprintf(text, sizeof text, "%s%s\ncolumns\n1\n1\n1\n1\n1\n", header, cases[i].clue);
        cr_assert(length > 0 && (size_t)length < sizeof text);
        assert_fails(text, NG_ERR_PARSE, 4, cases[i].column, cases[i].message);
    }
}

Test(io_read_non_errors, empty_line_inside_a_section)
{
    assert_fails("width 1\nheight 2\nrows\n1\n\n0\ncolumns\n1\n", NG_ERR_PARSE, 5, 1,
                 "empty line in 'rows' section");
}

Test(io_read_non_errors, clue_longer_than_its_line_is_inconsistent)
{
    assert_fails("width 5\nheight 3\nrows\n1\n0\n3,3\ncolumns\n1\n1\n1\n1\n1\n",
                 NG_ERR_INCONSISTENT, 6, 1, "row 3 clues need 7 cells, width is 5");
    assert_fails("width 1\nheight 2\nrows\n1\n1\ncolumns\n1,1\n", NG_ERR_INCONSISTENT, 7, 1,
                 "column 1 clues need 3 cells, height is 2");
}

Test(io_read_non_errors, different_totals_are_inconsistent)
{
    assert_fails("width 2\nheight 2\nrows\n1\n1\ncolumns\n2\n1\n", NG_ERR_INCONSISTENT, 9, 1,
                 "row clues sum to 2, column clues to 3");
}

Test(io_read_non_errors, text_values_must_be_quoted_and_closed)
{
    assert_fails("title Lettre\n", NG_ERR_PARSE, 1, 7, "expected a quoted value for 'title'");
    assert_fails("title \"Lettre\n", NG_ERR_PARSE, 1, 14, "unterminated value for 'title'");
    assert_fails("title \"a\" b\n", NG_ERR_PARSE, 1, 11, "unexpected text after 'title'");
}

Test(io_read_non_errors, text_value_above_255_bytes)
{
    char text[400] = "author \"";
    memset(text + 8, 'a', 256);
    memcpy(text + 8 + 256, "\"\n", 3);

    assert_fails(text, NG_ERR_PARSE, 1, 9, "'author' longer than 255 bytes");
}

Test(io_read_non_errors, byte_order_mark_is_rejected)
{
    assert_fails("\xEF\xBB\xBFwidth 1\n", NG_ERR_PARSE, 1, 1, "byte order mark");
}

Test(io_read_non_errors, lone_carriage_return_is_rejected)
{
    assert_fails("width 1\rheight 1\n", NG_ERR_PARSE, 1, 8, "carriage return");
}

Test(io_read_non_errors, sections_out_of_order)
{
    assert_fails("width 1\nheight 1\ncolumns\n1\nrows\n1\n", NG_ERR_PARSE, 3, 1,
                 "unexpected 'columns' before 'rows'");
    assert_fails("width 1\nheight 1\nrows\n1\ncolumns\n1\nrows\n", NG_ERR_PARSE, 7, 1,
                 "duplicate section 'rows'");
}

Test(io_read_non_errors, clue_line_in_the_header)
{
    assert_fails("width 1\n3\n", NG_ERR_PARSE, 2, 1, "expected a directive");
}

/* --- io_write_non ------------------------------------------------------------------------- */

static const char p_written[] = "title \"Lettre P\"\n"
                                "author \"nonogram-solver\"\n"
                                "width 5\n"
                                "height 5\n"
                                "\n"
                                "rows\n"
                                "3\n"
                                "1,1\n"
                                "3\n"
                                "1\n"
                                "1\n"
                                "\n"
                                "columns\n"
                                "5\n"
                                "1,1\n"
                                "3\n"
                                "0\n"
                                "0\n";

static ng_buffer *write_to_buffer(const ng_puzzle *puzzle, const ng_non_metadata *metadata)
{
    ng_buffer *buffer = nullptr;
    cr_assert_eq(ng_buffer_create(&ng_test_heap()->base, &buffer), NG_OK);
    const ng_writer w = ng_writer_from_buffer(buffer);
    cr_assert_eq(ng_io_write_non(puzzle, metadata, &w), NG_OK);
    return buffer;
}

Test(io_write_non, writes_the_canonical_text)
{
    read_result result = read_text(p_text);
    cr_assert_eq(result.status, NG_OK);

    ng_buffer *buffer = write_to_buffer(result.puzzle, result.metadata);

    cr_assert_str_eq(ng_buffer_data(buffer), p_written);
    ng_buffer_destroy(buffer);
    release(&result);
}

Test(io_write_non, without_metadata_only_dimensions_precede_the_clues)
{
    read_result result = read_text(p_text);
    cr_assert_eq(result.status, NG_OK);

    ng_buffer *buffer = write_to_buffer(result.puzzle, nullptr);

    cr_assert_eq(strncmp(ng_buffer_data(buffer), "width 5\nheight 5\n\nrows\n", 23), 0, "%s",
                 ng_buffer_data(buffer));
    ng_buffer_destroy(buffer);
    release(&result);
}

Test(io_write_non, output_reads_back_to_the_same_puzzle_and_metadata)
{
    const char *text = "title \"T\"\ncatalogue \"K\"\nwidth 2\nheight 1\nrows\n1\ncolumns\n1\n0\n"
                       "goal \"10\"\n";
    read_result first = read_text(text);
    cr_assert_eq(first.status, NG_OK, "%s", first.error.message);
    ng_buffer *buffer = write_to_buffer(first.puzzle, first.metadata);

    read_result second = read_text(ng_buffer_data(buffer));

    cr_assert_eq(second.status, NG_OK, "%s\n%s", second.error.message, ng_buffer_data(buffer));
    for (size_t f = 0; f < NG_NON_FIELD_COUNT; ++f) {
        const char *a = ng_non_metadata_get(first.metadata, (ng_non_field)f);
        const char *b = ng_non_metadata_get(second.metadata, (ng_non_field)f);
        cr_assert_eq(a == nullptr, b == nullptr, "field %zu", f);
        if (a != nullptr) {
            cr_assert_str_eq(a, b, "field %zu", f);
        }
    }
    cr_assert_eq(ng_puzzle_line_run_count(second.puzzle, 0), 1u);
    cr_assert_eq(ng_puzzle_line_run_count(second.puzzle, 2), 0u);
    ng_buffer_destroy(buffer);
    release(&first);
    release(&second);
}

Test(io_write_non, writer_failure_is_reported_at_every_point)
{
    read_result result = read_text(p_text);
    cr_assert_eq(result.status, NG_OK);
    const size_t total = sizeof p_written - 1u;

    for (size_t limit = 0; limit <= total; ++limit) {
        ng_test_sink sink;
        const ng_writer w = ng_test_failing_writer(&sink, limit);

        const ng_status status = ng_io_write_non(result.puzzle, result.metadata, &w);

        cr_assert_eq(status, limit < total ? NG_ERR_IO : NG_OK, "limit %zu", limit);
    }
    release(&result);
}

Test(io_write_non, nullptr_arguments_are_invalid)
{
    read_result result = read_text(p_text);
    cr_assert_eq(result.status, NG_OK);
    ng_test_sink sink;
    const ng_writer w = ng_test_failing_writer(&sink, SIZE_MAX);

    cr_assert_eq(ng_io_write_non(nullptr, nullptr, &w), NG_ERR_INVALID_ARGUMENT);
    cr_assert_eq(ng_io_write_non(result.puzzle, nullptr, nullptr), NG_ERR_INVALID_ARGUMENT);
    release(&result);
}

Test(io_write_non, metadata_accessors_treat_nullptr_as_absent)
{
    cr_assert_null(ng_non_metadata_get(nullptr, NG_NON_TITLE));
    ng_non_metadata_destroy(nullptr);
}

/* --- io_read_non_errors: remaining paths ---------------------------------------------------- */

Test(io_read_non_errors, value_glued_to_its_directive)
{
    assert_fails("width5\n", NG_ERR_PARSE, 1, 6, "invalid value for 'width'");
}

Test(io_read_non_errors, duplicate_columns_section)
{
    assert_fails("width 1\nheight 1\nrows\n1\ncolumns\n1\ncolumns\n", NG_ERR_PARSE, 7, 1,
                 "duplicate section 'columns'");
}

Test(io_read_non_errors, directive_between_the_sections)
{
    assert_fails("width 1\nheight 1\nrows\n1\ntitle \"x\"\ncolumns\n1\n", NG_ERR_PARSE, 5, 1,
                 "expected 'columns' after the row clues");
}

Test(io_read_non_errors, end_of_input_inside_the_rows)
{
    assert_fails("width 1\nheight 2\nrows\n1\n", NG_ERR_PARSE, 5, 1, "expected 2 row clues, got 1");
}

Test(io_read_non_errors, long_token_is_cut_in_the_message)
{
    char text[200] = "width 1\nheight 1\nrows\n";
    const size_t header = strlen(text);
    memset(text + header, '7', 100);
    text[header + 100] = 'x';

    assert_fails(text, NG_ERR_PARSE, 4, 1, "...' at position 1");
}

Test(io_read_non_errors, failure_without_an_error_report)
{
    ng_memory_source source;
    const char *text = "width 0\n";
    const ng_reader r = ng_reader_from_memory(&source, text, strlen(text));
    ng_puzzle *puzzle = nullptr;

    const ng_status status = ng_io_read_non(&ng_test_heap()->base, &r, &puzzle, nullptr, nullptr);

    cr_assert_eq(status, NG_ERR_PARSE);
    cr_assert_null(puzzle);
}

Test(io_read_non_errors, read_failure_inside_a_comment_or_a_clue_line)
{
    const char *text = "# a comment\nwidth 3\nheight 1\nrows\n1,1\ncolumns\n1\n0\n1\n";
    const size_t failure_points[] = {5, 36};

    for (size_t i = 0; i < sizeof failure_points / sizeof failure_points[0]; ++i) {
        ng_test_source source;
        const ng_reader r = ng_test_failing_reader(&source, text, strlen(text), failure_points[i]);
        ng_puzzle *puzzle = nullptr;
        ng_parse_error error = {};

        const ng_status status =
            ng_io_read_non(&ng_test_heap()->base, &r, &puzzle, nullptr, &error);

        cr_assert_eq(status, NG_ERR_IO, "failure at byte %zu: %s", failure_points[i],
                     error.message);
        cr_assert_null(puzzle);
    }
}
