#include <stddef.h>

#include "nonogram/io.h"
#include "nonogram/puzzle.h"
#include "nonogram/status.h"
#include "nonogram/stream.h"
#include "nonogram/types.h"

#include "core/internal/puzzle.h"
#include "io/internal/text.h"

/* Writes the canonical .non form read back by ng_io_read_non (P-IO2): text directives, the
 * dimensions, the row then column clues with commas, and the goal last. */

static const char *const header_fields[] = {"title", "author", "copyright", "license", "catalogue"};

static void write_text_directive(ngi_output *out, const char *name, const char *value)
{
    if (value == nullptr) {
        return;
    }
    ngi_output_text(out, name);
    ngi_output_text(out, " \"");
    ngi_output_text(out, value);
    ngi_output_text(out, "\"\n");
}

static void write_clues(ngi_output *out, const ng_puzzle *puzzle, ng_line_id first,
                        ng_line_id count)
{
    for (ng_line_id line = first; line < first + count; ++line) {
        const ng_run_count run_count = ng_puzzle_line_run_count(puzzle, line);
        const ng_dim *runs = ngi_puzzle_line_runs(puzzle, line);
        if (run_count == 0u) {
            ngi_output_char(out, '0');
        }
        for (ng_run_count i = 0; i < run_count; ++i) {
            if (i != 0u) {
                ngi_output_char(out, ',');
            }
            ngi_output_uint(out, runs[i]);
        }
        ngi_output_char(out, '\n');
    }
}

[[nodiscard]] ng_status ng_io_write_non(const ng_puzzle *puzzle, const ng_non_metadata *metadata,
                                        const ng_writer *writer)
{
    if (puzzle == nullptr || writer == nullptr) {
        return NG_ERR_INVALID_ARGUMENT;
    }
    ngi_output out;
    ngi_output_init(&out, writer);
    for (size_t f = 0; f < sizeof header_fields / sizeof header_fields[0]; ++f) {
        write_text_directive(&out, header_fields[f],
                             ng_non_metadata_get(metadata, (ng_non_field)f));
    }
    const ng_dim width = ng_puzzle_width(puzzle);
    const ng_dim height = ng_puzzle_height(puzzle);
    ngi_output_text(&out, "width ");
    ngi_output_uint(&out, width);
    ngi_output_text(&out, "\nheight ");
    ngi_output_uint(&out, height);
    ngi_output_text(&out, "\n\nrows\n");
    write_clues(&out, puzzle, 0, height);
    ngi_output_text(&out, "\ncolumns\n");
    write_clues(&out, puzzle, height, width);
    const char *goal = ng_non_metadata_get(metadata, NG_NON_GOAL);
    if (goal != nullptr) {
        ngi_output_char(&out, '\n');
        write_text_directive(&out, "goal", goal);
    }
    return ngi_output_finish(&out);
}
