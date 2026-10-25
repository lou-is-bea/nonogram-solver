#include <stddef.h>
#include <stdint.h>

#include "nonogram/grid.h"
#include "nonogram/puzzle.h"
#include "nonogram/render.h"
#include "nonogram/status.h"
#include "nonogram/stream.h"
#include "nonogram/types.h"

#include "core/internal/output.h"
#include "core/internal/puzzle.h"

/* Text rendering, layout documented in render.h. Output is streamed line by line through
 * ngi_output: nothing is allocated, whatever the size of the grid. */

static constexpr uint64_t decimal_base = 10;
static constexpr size_t cell_states = (size_t)NG_CELL_FILLED + 1u; /* ng_cell values */

typedef struct glyphs {
    const char *filled;
    const char *empty;
    const char *unknown;
    const char *top_left;
    const char *top_right;
    const char *bottom_left;
    const char *bottom_right;
    const char *horizontal;
    const char *vertical;
} glyphs;

static const glyphs ascii_glyphs = {"#", ".", "?", "+", "+", "+", "+", "-", "|"};
static const glyphs unicode_glyphs = {"█", "·", "?", "┌", "┐", "└", "┘", "─", "│"};

typedef struct layout {
    ng_dim width;
    ng_dim height;
    bool clues;
    size_t row_clue_width;  /* characters of the widest row clue */
    size_t cell_width;      /* characters of a cell symbol or column clue number */
    ng_run_count clue_rows; /* lines of the column clue block */
} layout;

/* Output that drops trailing spaces: spaces are only written once something follows them. */
typedef struct line_output {
    ngi_output out;
    size_t pending_spaces;
} line_output;

static void pad(line_output *o, size_t count)
{
    o->pending_spaces += count;
}

static void put(line_output *o, const char *text)
{
    for (; o->pending_spaces != 0u; --o->pending_spaces) {
        ngi_output_char(&o->out, ' ');
    }
    ngi_output_text(&o->out, text);
}

static void put_uint(line_output *o, uint64_t value)
{
    put(o, "");
    ngi_output_uint(&o->out, value);
}

static void end_line(line_output *o)
{
    o->pending_spaces = 0;
    ngi_output_char(&o->out, '\n');
}

static size_t digits(uint64_t value)
{
    size_t count = 1;
    while (value >= decimal_base) {
        value /= decimal_base;
        ++count;
    }
    return count;
}

/* Characters of a clue written with single spaces; "0" for an empty clue. */
static size_t clue_length(const ng_puzzle *puzzle, ng_line_id line)
{
    const ng_run_count count = ng_puzzle_line_run_count(puzzle, line);
    const ng_dim *runs = ngi_puzzle_line_runs(puzzle, line);
    size_t length = count == 0u ? 1u : (size_t)count - 1u;
    for (ng_run_count i = 0; i < count; ++i) {
        length += digits(runs[i]);
    }
    return length;
}

static layout compute_layout(const ng_puzzle *puzzle, bool clues)
{
    layout l = {.width = ng_puzzle_width(puzzle),
                .height = ng_puzzle_height(puzzle),
                .clues = clues,
                .cell_width = 1,
                .clue_rows = 1};
    if (!clues) {
        return l;
    }
    for (ng_line_id row = 0; row < l.height; ++row) {
        const size_t length = clue_length(puzzle, row);
        l.row_clue_width = length > l.row_clue_width ? length : l.row_clue_width;
    }
    for (ng_dim col = 0; col < l.width; ++col) {
        const ng_line_id line = l.height + col;
        const ng_run_count count = ng_puzzle_line_run_count(puzzle, line);
        const ng_dim *runs = ngi_puzzle_line_runs(puzzle, line);
        l.clue_rows = count > l.clue_rows ? count : l.clue_rows;
        for (ng_run_count i = 0; i < count; ++i) {
            const size_t width = digits(runs[i]);
            l.cell_width = width > l.cell_width ? width : l.cell_width;
        }
    }
    return l;
}

/* Column clues, bottom-aligned: block line @p r shows run (count - clue_rows + r). */
static void write_column_clues(line_output *o, const ng_puzzle *puzzle, const layout *l)
{
    for (ng_run_count r = 0; r < l->clue_rows; ++r) {
        pad(o, l->row_clue_width + 2u);
        for (ng_dim col = 0; col < l->width; ++col) {
            const ng_line_id line = l->height + col;
            const ng_run_count count = ng_puzzle_line_run_count(puzzle, line);
            const ng_run_count first = l->clue_rows - (count == 0u ? 1u : count);
            pad(o, 1);
            if (r < first) {
                pad(o, l->cell_width);
                continue;
            }
            const uint64_t value = count == 0u ? 0u : ngi_puzzle_line_runs(puzzle, line)[r - first];
            pad(o, l->cell_width - digits(value));
            put_uint(o, value);
        }
        end_line(o);
    }
}

static void write_border(line_output *o, const layout *l, const char *left, const char *right,
                         const char *horizontal)
{
    if (l->clues) {
        pad(o, l->row_clue_width + 1u);
    }
    put(o, left);
    /* Each cell takes a separating space plus its width; one more space before the frame. */
    const uint64_t inner = ((uint64_t)l->width * (l->cell_width + 1u)) + 1u;
    for (uint64_t i = 0; i < inner; ++i) {
        put(o, horizontal);
    }
    put(o, right);
    end_line(o);
}

static void write_row_clue(line_output *o, const ng_puzzle *puzzle, const layout *l, ng_line_id row)
{
    const ng_run_count count = ng_puzzle_line_run_count(puzzle, row);
    const ng_dim *runs = ngi_puzzle_line_runs(puzzle, row);
    pad(o, l->row_clue_width - clue_length(puzzle, row));
    if (count == 0u) {
        put_uint(o, 0);
    }
    for (ng_run_count i = 0; i < count; ++i) {
        if (i != 0u) {
            pad(o, 1);
        }
        put_uint(o, runs[i]);
    }
    pad(o, 1);
}

static const char *symbol(const glyphs *g, const ng_grid *grid, ng_dim row, ng_dim col)
{
    if (grid == nullptr) {
        return g->unknown;
    }
    switch (ng_grid_get(grid, row, col)) {
    case NG_CELL_FILLED:
        return g->filled;
    case NG_CELL_EMPTY:
        return g->empty;
    case NG_CELL_UNKNOWN:
        break;
    }
    return g->unknown;
}

static void write_rows(line_output *o, const ng_puzzle *puzzle, const ng_grid *grid,
                       const layout *l, const glyphs *g)
{
    for (ng_dim row = 0; row < l->height; ++row) {
        if (l->clues) {
            write_row_clue(o, puzzle, l, row);
        }
        put(o, g->vertical);
        for (ng_dim col = 0; col < l->width; ++col) {
            pad(o, l->cell_width);
            put(o, symbol(g, grid, row, col));
        }
        pad(o, 1);
        put(o, g->vertical);
        end_line(o);
    }
}

static bool matches(const ng_puzzle *puzzle, const ng_grid *grid)
{
    return grid == nullptr || (ng_grid_width(grid) == ng_puzzle_width(puzzle) &&
                               ng_grid_height(grid) == ng_puzzle_height(puzzle));
}

[[nodiscard]] ng_status ng_render_text(const ng_puzzle *puzzle, const ng_grid *grid,
                                       const ng_render_options *options, const ng_writer *writer)
{
    if (puzzle == nullptr || writer == nullptr || !matches(puzzle, grid)) {
        return NG_ERR_INVALID_ARGUMENT;
    }
    const ng_render_options defaults = {};
    const ng_render_options *o = options == nullptr ? &defaults : options;
    const glyphs *g = o->unicode ? &unicode_glyphs : &ascii_glyphs;
    const layout l = compute_layout(puzzle, !o->hide_clues);
    line_output out = {};
    ngi_output_init(&out.out, writer);
    if (l.clues) {
        write_column_clues(&out, puzzle, &l);
    }
    write_border(&out, &l, g->top_left, g->top_right, g->horizontal);
    write_rows(&out, puzzle, grid, &l, g);
    write_border(&out, &l, g->bottom_left, g->bottom_right, g->horizontal);
    return ngi_output_finish(&out.out);
}

[[nodiscard]] ng_status ng_render_summary(const ng_puzzle *puzzle, const ng_grid *grid,
                                          const ng_writer *writer)
{
    if (puzzle == nullptr || writer == nullptr || !matches(puzzle, grid)) {
        return NG_ERR_INVALID_ARGUMENT;
    }
    ng_cell_index announced = 0;
    for (ng_line_id row = 0; row < ng_puzzle_height(puzzle); ++row) {
        const ng_dim *runs = ngi_puzzle_line_runs(puzzle, row);
        for (ng_run_count i = 0; i < ng_puzzle_line_run_count(puzzle, row); ++i) {
            announced += runs[i]; /* bounded by width x height: cannot overflow */
        }
    }
    ngi_output out;
    ngi_output_init(&out, writer);
    ngi_output_text(&out, "puzzle: ");
    ngi_output_uint(&out, ng_puzzle_width(puzzle));
    ngi_output_text(&out, " x ");
    ngi_output_uint(&out, ng_puzzle_height(puzzle));
    ngi_output_text(&out, ", ");
    ngi_output_uint(&out, ng_puzzle_line_count(puzzle));
    ngi_output_text(&out, " lines, ");
    ngi_output_uint(&out, ng_puzzle_total_runs(puzzle));
    ngi_output_text(&out, " runs, ");
    ngi_output_uint(&out, announced);
    ngi_output_text(&out, " filled cells\n");
    if (grid != nullptr) {
        ng_cell_index counts[cell_states] = {};
        for (ng_dim row = 0; row < ng_grid_height(grid); ++row) {
            for (ng_dim col = 0; col < ng_grid_width(grid); ++col) {
                ++counts[ng_grid_get(grid, row, col)];
            }
        }
        ngi_output_text(&out, "grid: ");
        ngi_output_uint(&out, counts[NG_CELL_FILLED]);
        ngi_output_text(&out, " filled, ");
        ngi_output_uint(&out, counts[NG_CELL_EMPTY]);
        ngi_output_text(&out, " empty, ");
        ngi_output_uint(&out, counts[NG_CELL_UNKNOWN]);
        ngi_output_text(&out, " unknown\n");
    }
    return ngi_output_finish(&out);
}
