#include <inttypes.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include "nonogram/allocator.h"
#include "nonogram/grid.h"
#include "nonogram/io.h"
#include "nonogram/status.h"
#include "nonogram/stream.h"
#include "nonogram/types.h"

#include "io/internal/char_source.h"
#include "io/internal/text.h"

/* .sol grids (docs/FILE_FORMATS.md §2). The dimensions are only known at the end of the
 * input, so the cells are first accumulated in an ng_buffer (one byte each), then copied into
 * the grid. */

static constexpr size_t stage_size = 256; /* cells appended to the buffer at once */

typedef struct sol_reader {
    const ng_allocator *allocator;
    ngi_char_source source;
    ng_parse_error *error;
    ng_status status;
    ng_buffer *cells;
    ng_writer cells_writer;
    char stage[stage_size];
    size_t staged;
    uint64_t width; /* 0 until the first line ends */
    uint64_t height;
    uint64_t row_length;
} sol_reader;

[[gnu::format(printf, 3, 4)]] static ng_status fail(sol_reader *r, ng_status status,
                                                    const char *format, ...)
{
    if (r->status == NG_OK) {
        r->status = status;
        va_list args;
        va_start(args, format);
        ngi_parse_error_vset(r->error, r->source.line, r->source.column, format, args);
        va_end(args);
    }
    return r->status;
}

/* A carriage return is only valid as the first half of "\r\n"; it is reported at its own
 * position. */
static void skip_carriage_return(sol_reader *r)
{
    const uint64_t line = r->source.line;
    const uint64_t column = r->source.column;
    ngi_char_source_advance(&r->source);
    const int next = ngi_char_source_peek(&r->source);
    if (next != '\n' && next != NGI_CHAR_FAILED && r->status == NG_OK) {
        r->status = NG_ERR_PARSE;
        ngi_parse_error_set(r->error, line, column, "carriage return not followed by a line feed");
    }
}

static ng_status flush_stage(sol_reader *r)
{
    const ng_status status = r->cells_writer.write(r->cells_writer.ctx, r->stage, r->staged);
    r->staged = 0;
    return status == NG_OK ? NG_OK : fail(r, status, "%s", ng_status_str(status));
}

static ng_status add_cell(sol_reader *r, char c)
{
    if (r->width != 0u && r->row_length == r->width) {
        return fail(r, NG_ERR_PARSE, "line %" PRIu64 " has more than %" PRIu64 " cells",
                    r->height + 1u, r->width);
    }
    if (r->row_length == NG_DIM_MAX) {
        return fail(r, NG_ERR_LIMIT, "line longer than %" NG_PRI_DIM " cells", NG_DIM_MAX);
    }
    r->stage[r->staged++] = c;
    ++r->row_length;
    return r->staged == sizeof r->stage ? flush_stage(r) : NG_OK;
}

static ng_status end_row(sol_reader *r)
{
    if (r->row_length == 0u) {
        return fail(r, NG_ERR_PARSE, "empty line");
    }
    if (r->width == 0u) {
        r->width = r->row_length;
    } else if (r->row_length != r->width) {
        return fail(r, NG_ERR_PARSE, "line %" PRIu64 " has %" PRIu64 " cells, expected %" PRIu64,
                    r->height + 1u, r->row_length, r->width);
    }
    if (r->height == NG_DIM_MAX) {
        return fail(r, NG_ERR_LIMIT, "more than %" NG_PRI_DIM " lines", NG_DIM_MAX);
    }
    ++r->height;
    r->row_length = 0;
    return NG_OK;
}

/* Handles one byte; returns false at the end of the input or after a failure. */
static bool step(sol_reader *r)
{
    const int c = ngi_char_source_peek(&r->source);
    if (c == NGI_CHAR_FAILED) {
        fail(r, r->source.status, "read error");
        return false;
    }
    if (c == NGI_CHAR_END) {
        if (r->height == 0u && r->row_length == 0u) {
            fail(r, NG_ERR_PARSE, "empty grid");
        } else if (r->row_length != 0u) {
            end_row(r); /* last line without a final newline */
        }
        return false;
    }
    if (c == '#' || c == '.' || c == '?') {
        add_cell(r, (char)c);
    } else if (c == '\n') {
        end_row(r);
    } else if (c == '\r') {
        skip_carriage_return(r);
        return r->status == NG_OK;
    } else {
        char text[NGI_TOKEN_TEXT_SIZE];
        const char byte = (char)c;
        ngi_escape(text, &byte, 1, false);
        fail(r, NG_ERR_PARSE, "invalid character '%s'", text);
    }
    if (r->status == NG_OK) {
        ngi_char_source_advance(&r->source);
    }
    return r->status == NG_OK;
}

static ng_status build_grid(sol_reader *r, ng_grid **out)
{
    ng_status status = ng_grid_create(r->allocator, (ng_dim)r->width, (ng_dim)r->height, out);
    if (status != NG_OK) {
        return fail(r, status, "%s", ng_status_str(status));
    }
    const char *cells = ng_buffer_data(r->cells);
    for (ng_dim row = 0; row < (ng_dim)r->height; ++row) {
        for (ng_dim col = 0; col < (ng_dim)r->width; ++col) {
            const char c = cells[((size_t)row * (size_t)r->width) + col];
            ng_grid_set(*out, row, col,
                        c == '#' ? NG_CELL_FILLED : (c == '.' ? NG_CELL_EMPTY : NG_CELL_UNKNOWN));
        }
    }
    return NG_OK;
}

[[nodiscard]] ng_status ng_io_read_sol(const ng_allocator *allocator, const ng_reader *reader,
                                       ng_grid **out, ng_parse_error *error)
{
    if (out != nullptr) {
        *out = nullptr;
    }
    if (allocator == nullptr || reader == nullptr || out == nullptr) {
        return NG_ERR_INVALID_ARGUMENT;
    }
    if (error != nullptr) {
        *error = (ng_parse_error){};
    }
    sol_reader r = {.allocator = allocator, .error = error, .status = NG_OK};
    if (ngi_char_source_init(&r.source, allocator, reader) != NG_OK) {
        ngi_parse_error_set(error, 1, 1, "out of memory");
        return NG_ERR_OUT_OF_MEMORY;
    }
    if (ng_buffer_create(allocator, &r.cells) != NG_OK) {
        fail(&r, NG_ERR_OUT_OF_MEMORY, "out of memory");
    } else {
        r.cells_writer = ng_writer_from_buffer(r.cells);
        while (step(&r)) {
        }
        if (r.status == NG_OK && flush_stage(&r) == NG_OK) {
            build_grid(&r, out);
        }
    }
    ngi_char_source_release(&r.source, allocator);
    ng_buffer_destroy(r.cells);
    return r.status;
}

[[nodiscard]] ng_status ng_io_write_sol(const ng_grid *grid, const ng_writer *writer)
{
    if (grid == nullptr || writer == nullptr) {
        return NG_ERR_INVALID_ARGUMENT;
    }
    static const char symbols[] = {
        [NG_CELL_UNKNOWN] = '?', [NG_CELL_EMPTY] = '.', [NG_CELL_FILLED] = '#'};
    ngi_output out;
    ngi_output_init(&out, writer);
    for (ng_dim row = 0; row < ng_grid_height(grid) && out.status == NG_OK; ++row) {
        for (ng_dim col = 0; col < ng_grid_width(grid); ++col) {
            ngi_output_char(&out, symbols[ng_grid_get(grid, row, col)]);
        }
        ngi_output_char(&out, '\n');
    }
    return ngi_output_finish(&out);
}
