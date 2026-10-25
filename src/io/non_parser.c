#include "io/internal/non_parser.h"

#include <inttypes.h>
#include <stdckdint.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "nonogram/allocator.h"
#include "nonogram/io.h"
#include "nonogram/puzzle.h"
#include "nonogram/status.h"
#include "nonogram/stream.h"
#include "nonogram/types.h"

#include "io/internal/char_source.h"
#include "io/internal/non_metadata.h"
#include "io/internal/text.h"

/* Streaming reader of the .non subset of docs/FILE_FORMATS.md §1. The input is consumed byte
 * by byte through ngi_char_source, so no line is ever held in memory: a clue line of millions
 * of runs costs nothing beyond the runs themselves. */

static constexpr size_t initial_run_capacity = 64; /* then doubles */
static constexpr unsigned char utf8_bom_first = 0xEF;

/* --- sections --------------------------------------------------------------------------- */

static ng_status start_rows(ngi_non_parser *p, const ngi_non_token *word)
{
    if (p->section != NGI_NON_SECTION_HEADER) {
        return ngi_non_fail_at(p, NG_ERR_PARSE, word->line, word->column,
                               "duplicate section 'rows'");
    }
    if (!p->has_width || !p->has_height) {
        return ngi_non_fail_at(p, NG_ERR_PARSE, word->line, word->column, "missing '%s' directive",
                               p->has_width ? "height" : "width");
    }
    if (ngi_non_expect_line_end(p, "rows") != NG_OK) {
        return p->status;
    }
    const uint64_t offset_count = (uint64_t)p->height + p->width + 1u;
    if (ckd_mul(&p->offsets_bytes, offset_count, sizeof(ng_run_index))) {
        return ngi_non_fail_at(p, NG_ERR_LIMIT, word->line, word->column, "puzzle too large");
    }
    p->offsets = p->allocator->alloc(p->allocator->ctx, p->offsets_bytes, alignof(ng_run_index));
    if (p->offsets == nullptr) {
        return ngi_non_fail_at(p, NG_ERR_OUT_OF_MEMORY, word->line, word->column, "out of memory");
    }
    p->offsets[0] = 0;
    p->section = NGI_NON_SECTION_ROWS;
    p->remaining = p->height;
    return NG_OK;
}

static ng_status start_columns(ngi_non_parser *p, const ngi_non_token *word)
{
    if (p->section == NGI_NON_SECTION_HEADER) {
        return ngi_non_fail_at(p, NG_ERR_PARSE, word->line, word->column,
                               "unexpected 'columns' before 'rows'");
    }
    if (p->section != NGI_NON_SECTION_ROWS) {
        return ngi_non_fail_at(p, NG_ERR_PARSE, word->line, word->column,
                               "duplicate section 'columns'");
    }
    if (ngi_non_expect_line_end(p, "columns") != NG_OK) {
        return p->status;
    }
    p->section = NGI_NON_SECTION_COLUMNS;
    p->remaining = p->width;
    return NG_OK;
}

/* A line outside the clue sections: blank, a directive or a section keyword. */
static ng_status parse_keyword_line(ngi_non_parser *p)
{
    const int c = ngi_non_skip_spaces(p);
    if (c == NGI_NON_CHAR_ERROR) {
        return p->status;
    }
    if (ngi_non_is_line_end(c)) {
        return ngi_non_expect_line_end(p, "blank line");
    }
    if (!ngi_non_is_letter(c)) {
        const bool after_clues =
            p->section == NGI_NON_SECTION_ROWS || p->section == NGI_NON_SECTION_TRAILER;
        if (after_clues && c >= '0' && c <= '9') {
            const bool rows = p->section == NGI_NON_SECTION_ROWS;
            return ngi_non_fail_at(p, NG_ERR_PARSE, p->source.line, 1,
                                   "expected %" NG_PRI_DIM " %s clues, got more",
                                   rows ? p->height : p->width, rows ? "row" : "column");
        }
        return ngi_non_fail_at(p, NG_ERR_PARSE, p->source.line, p->source.column,
                               "expected a directive");
    }
    ngi_non_token word;
    if (ngi_non_read_token(p, &word, true) == NGI_NON_CHAR_ERROR) {
        return p->status;
    }
    if (ngi_non_token_is(&word, "rows")) {
        return start_rows(p, &word);
    }
    if (ngi_non_token_is(&word, "columns")) {
        return start_columns(p, &word);
    }
    if (p->section == NGI_NON_SECTION_ROWS) {
        return ngi_non_fail_at(p, NG_ERR_PARSE, word.line, word.column,
                               "expected 'columns' after the row clues");
    }
    return ngi_non_parse_directive(p, &word);
}

/* --- clue lines ------------------------------------------------------------------------- */

typedef struct clue_line {
    uint64_t line;      /* line of the file */
    ng_dim length;      /* cells of the grid line */
    bool is_row;        /* row or column of the grid */
    uint64_t position;  /* 1-based index of the current clue in the line */
    ng_cell_index need; /* cells needed so far: runs plus one gap between runs */
    bool exceeded;      /* need > length: runs are no longer stored */
} clue_line;

static ng_status push_run(ngi_non_parser *p, ng_dim run)
{
    if (p->run_count == p->run_capacity) {
        size_t capacity = p->run_capacity == 0u ? initial_run_capacity : p->run_capacity;
        size_t bytes = 0;
        if ((p->run_capacity != 0u && ckd_mul(&capacity, capacity, (size_t)2)) ||
            ckd_mul(&bytes, capacity, sizeof(ng_dim))) {
            return ngi_non_fail_at(p, NG_ERR_LIMIT, p->source.line, 1, "puzzle too large");
        }
        ng_dim *runs = p->allocator->alloc(p->allocator->ctx, bytes, alignof(ng_dim));
        if (runs == nullptr) {
            return ngi_non_fail_at(p, NG_ERR_OUT_OF_MEMORY, p->source.line, 1, "out of memory");
        }
        if (p->runs != nullptr) {
            memcpy(runs, p->runs, (size_t)p->run_count * sizeof(ng_dim));
            p->allocator->free(p->allocator->ctx, p->runs, p->run_capacity * sizeof(ng_dim));
        }
        p->runs = runs;
        p->run_capacity = capacity;
    }
    p->runs[p->run_count++] = run;
    return NG_OK;
}

static ng_status add_run(ngi_non_parser *p, clue_line *cl, ng_dim run)
{
    ng_cell_index *total = &p->totals[cl->is_row ? 0 : 1];
    if (ckd_add(total, *total, (ng_cell_index)run)) {
        *total = UINT64_MAX;
    }
    const ng_cell_index gap = cl->position == 1u ? 0u : 1u;
    if (ckd_add(&cl->need, cl->need, gap) || ckd_add(&cl->need, cl->need, (ng_cell_index)run)) {
        cl->need = UINT64_MAX;
    }
    cl->exceeded = cl->need > cl->length;
    return cl->exceeded ? NG_OK : push_run(p, run);
}

/* Reads one clue and the separator after it. Sets *done at the end of the line. */
static ng_status parse_clue(ngi_non_parser *p, clue_line *cl, bool *done)
{
    ngi_non_token t;
    int c = ngi_non_read_token(p, &t, false);
    if (c == NGI_NON_CHAR_ERROR) {
        return p->status;
    }
    if (t.size == 0u) {
        return ngi_non_fail_at(p, NG_ERR_PARSE, t.line, t.column,
                               c == ',' ? "invalid clue ',' at position %" PRIu64
                                        : "missing clue at position %" PRIu64,
                               cl->position);
    }
    uint64_t value = 0;
    const bool is_number = ngi_non_token_number(&t, &value);
    c = ngi_non_skip_spaces(p);
    if (c == NGI_NON_CHAR_ERROR) {
        return p->status;
    }
    *done = ngi_non_is_line_end(c);
    const bool lone_zero = is_number && value == 0u && cl->position == 1u && *done;
    if (!lone_zero && (!is_number || value == 0u || value > NG_DIM_MAX)) {
        return ngi_non_fail_at(p, NG_ERR_PARSE, t.line, t.column,
                               "invalid clue '%s' at position %" PRIu64, ngi_non_text_of(&t).text,
                               cl->position);
    }
    if (!lone_zero && add_run(p, cl, (ng_dim)value) != NG_OK) {
        return p->status;
    }
    if (c == ',') {
        ngi_non_advance(p);
        ngi_non_skip_spaces(p);
    }
    ++cl->position;
    return p->status;
}

static ng_status check_line_fits(ngi_non_parser *p, const clue_line *cl)
{
    if (!cl->exceeded) {
        return NG_OK;
    }
    const ng_line_id index = cl->is_row ? p->next_line : p->next_line - p->height;
    return ngi_non_fail_at(p, NG_ERR_INCONSISTENT, cl->line, 1,
                           "%s %" NG_PRI_LINE_ID " clues need %" NG_PRI_CELL_INDEX
                           " cells, %s is %" NG_PRI_DIM,
                           cl->is_row ? "row" : "column", index + 1u, cl->need,
                           cl->is_row ? "width" : "height", cl->length);
}

static ng_status parse_clue_line(ngi_non_parser *p)
{
    const bool is_row = p->section == NGI_NON_SECTION_ROWS;
    const char *kind = is_row ? "row" : "column";
    const ng_dim expected = is_row ? p->height : p->width;
    const uint64_t line = p->source.line;
    const int c = ngi_non_skip_spaces(p);
    if (c == NGI_NON_CHAR_ERROR) {
        return p->status;
    }
    if (ngi_non_is_line_end(c)) {
        return ngi_non_fail_at(p, NG_ERR_PARSE, line, 1,
                               "empty line in '%ss' section (write 0 for a line without runs)",
                               kind);
    }
    if (ngi_non_is_letter(c)) {
        return ngi_non_fail_at(p, NG_ERR_PARSE, line, 1,
                               "expected %" NG_PRI_DIM " %s clues, got %" NG_PRI_DIM, expected,
                               kind, expected - p->remaining);
    }
    clue_line cl = {
        .line = line, .length = is_row ? p->width : p->height, .is_row = is_row, .position = 1};
    p->offsets[p->next_line] = p->run_count;
    for (bool done = false; !done;) {
        if (parse_clue(p, &cl, &done) != NG_OK) {
            return p->status;
        }
    }
    if (check_line_fits(p, &cl) != NG_OK || ngi_non_expect_line_end(p, "clues") != NG_OK) {
        return p->status;
    }
    p->offsets[++p->next_line] = p->run_count;
    if (--p->remaining == 0u && !is_row) {
        p->section = NGI_NON_SECTION_TRAILER;
    }
    return NG_OK;
}

/* --- whole file ------------------------------------------------------------------------- */

static ng_status check_complete(ngi_non_parser *p)
{
    const uint64_t line = p->source.line;
    const uint64_t column = p->source.column;
    switch (p->section) {
    case NGI_NON_SECTION_HEADER:
        return ngi_non_fail_at(p, NG_ERR_PARSE, line, column,
                               !p->has_width    ? "missing 'width' directive"
                               : !p->has_height ? "missing 'height' directive"
                                                : "missing 'rows' section");
    case NGI_NON_SECTION_ROWS:
        if (p->remaining != 0u) {
            return ngi_non_fail_at(p, NG_ERR_PARSE, line, column,
                                   "expected %" NG_PRI_DIM " row clues, got %" NG_PRI_DIM,
                                   p->height, p->height - p->remaining);
        }
        return ngi_non_fail_at(p, NG_ERR_PARSE, line, column, "missing 'columns' section");
    case NGI_NON_SECTION_COLUMNS:
        return ngi_non_fail_at(p, NG_ERR_PARSE, line, column,
                               "expected %" NG_PRI_DIM " column clues, got %" NG_PRI_DIM, p->width,
                               p->width - p->remaining);
    case NGI_NON_SECTION_TRAILER:
        break;
    }
    if (p->totals[0] != p->totals[1]) {
        return ngi_non_fail_at(p, NG_ERR_INCONSISTENT, line, column,
                               "row clues sum to %" NG_PRI_CELL_INDEX
                               ", column clues to %" NG_PRI_CELL_INDEX,
                               p->totals[0], p->totals[1]);
    }
    return NG_OK;
}

static ng_status parse_lines(ngi_non_parser *p)
{
    if (ngi_char_source_peek(&p->source) == utf8_bom_first) {
        return ngi_non_fail_at(p, NG_ERR_PARSE, 1, 1,
                               "byte order mark not allowed (UTF-8 without BOM)");
    }
    while (p->status == NG_OK) {
        const int c = ngi_non_peek(p);
        if (c == NGI_NON_CHAR_ERROR) {
            break;
        }
        if (c == NGI_CHAR_END) {
            return check_complete(p);
        }
        if (c == '#') {
            ngi_non_skip_comment(p);
        } else if ((p->section == NGI_NON_SECTION_ROWS || p->section == NGI_NON_SECTION_COLUMNS) &&
                   p->remaining != 0u) {
            parse_clue_line(p);
        } else {
            parse_keyword_line(p);
        }
    }
    return p->status;
}

static ng_status build_puzzle(ngi_non_parser *p, ng_puzzle **out)
{
    const ng_status status =
        ng_puzzle_create(p->allocator, p->width, p->height, p->runs, p->offsets, out);
    if (status != NG_OK) {
        return ngi_non_fail_at(p, status, p->source.line, p->source.column, "%s",
                               ng_status_str(status));
    }
    return NG_OK;
}

static void release_parser(ngi_non_parser *p)
{
    ngi_char_source_release(&p->source, p->allocator);
    if (p->offsets != nullptr) {
        p->allocator->free(p->allocator->ctx, p->offsets, p->offsets_bytes);
    }
    if (p->runs != nullptr) {
        p->allocator->free(p->allocator->ctx, p->runs, p->run_capacity * sizeof(ng_dim));
    }
}

[[nodiscard]] ng_status ng_io_read_non(const ng_allocator *allocator, const ng_reader *reader,
                                       ng_puzzle **out, ng_non_metadata **metadata,
                                       ng_parse_error *error)
{
    if (out != nullptr) {
        *out = nullptr;
    }
    if (metadata != nullptr) {
        *metadata = nullptr;
    }
    if (allocator == nullptr || reader == nullptr || out == nullptr) {
        return NG_ERR_INVALID_ARGUMENT;
    }
    if (error != nullptr) {
        *error = (ng_parse_error){};
    }
    ngi_non_parser p = {.allocator = allocator, .error = error, .status = NG_OK};
    if (ngi_char_source_init(&p.source, allocator, reader) != NG_OK) {
        return ngi_non_fail_at(&p, NG_ERR_OUT_OF_MEMORY, 1, 1, "out of memory");
    }
    if (metadata != nullptr && ngi_non_metadata_create(allocator, &p.metadata) != NG_OK) {
        ngi_non_fail_at(&p, NG_ERR_OUT_OF_MEMORY, 1, 1, "out of memory");
    }
    if (p.status == NG_OK && parse_lines(&p) == NG_OK) {
        build_puzzle(&p, out);
    }
    release_parser(&p);
    if (p.status != NG_OK) {
        ng_non_metadata_destroy(p.metadata);
        return p.status;
    }
    if (metadata != nullptr) {
        *metadata = p.metadata;
    }
    return NG_OK;
}
