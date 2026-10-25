#include <inttypes.h>
#include <stdarg.h>
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

static constexpr size_t text_value_max = 255;      /* FILE_FORMATS.md §1.3 */
static constexpr size_t text_stage_size = 256;     /* bytes of a value appended at once */
static constexpr size_t initial_run_capacity = 64; /* then doubles */
static constexpr int char_error = -3;              /* failure already recorded */
static constexpr unsigned char utf8_bom_first = 0xEF;
static constexpr int first_printable = 0x20;
static constexpr uint64_t decimal_base = 10;

typedef enum section : uint8_t {
    SECTION_HEADER,  /* directives before 'rows' */
    SECTION_ROWS,    /* row clues, then blank lines or comments until 'columns' */
    SECTION_COLUMNS, /* column clues */
    SECTION_TRAILER, /* directives after the clues */
} section;

typedef struct non_parser {
    const ng_allocator *allocator;
    ngi_char_source source;
    ng_parse_error *error;
    ng_status status;
    ng_non_metadata *metadata; /* nullptr when the caller discards the metadata */
    bool seen[NG_NON_FIELD_COUNT];
    bool has_width;
    bool has_height;
    ng_dim width;
    ng_dim height;
    section section;
    ng_line_id next_line; /* unified id of the next clue line */
    ng_line_id remaining; /* clue lines still expected in the current section */
    ng_run_index *offsets;
    size_t offsets_bytes;
    ng_dim *runs;
    ng_run_index run_count;
    size_t run_capacity;
    ng_cell_index totals[2]; /* filled cells announced by the rows, by the columns */
} non_parser;

/* The first bytes of a token, for numbers and messages, and where it starts. */
typedef struct token {
    char raw[NGI_TOKEN_TEXT_SIZE];
    size_t size;
    bool truncated;
    uint64_t line;
    uint64_t column;
} token;

static const char *const field_names[NG_NON_FIELD_COUNT] = {
    [NG_NON_TITLE] = "title",         [NG_NON_AUTHOR] = "author",
    [NG_NON_COPYRIGHT] = "copyright", [NG_NON_LICENSE] = "license",
    [NG_NON_CATALOGUE] = "catalogue", [NG_NON_GOAL] = "goal",
};

/* --- failures and characters ------------------------------------------------------------ */

/* Records the first failure only: what follows it is a consequence. */
[[gnu::format(printf, 5, 6)]] static ng_status
fail_at(non_parser *p, ng_status status, uint64_t line, uint64_t column, const char *format, ...)
{
    if (p->status == NG_OK) {
        p->status = status;
        va_list args;
        va_start(args, format);
        ngi_parse_error_vset(p->error, line, column, format, args);
        va_end(args);
    }
    return p->status;
}

static bool is_letter(int c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static bool is_line_end(int c)
{
    return c == '\n' || c == NGI_CHAR_END;
}

/* Current byte, with "\r\n" seen as '\n'. Returns char_error once a read failure or a lone
 * carriage return has been recorded. */
static int peek(non_parser *p)
{
    int c = ngi_char_source_peek(&p->source);
    if (c == '\r') {
        const uint64_t line = p->source.line;
        const uint64_t column = p->source.column;
        ngi_char_source_advance(&p->source);
        c = ngi_char_source_peek(&p->source);
        if (c != '\n' && c != NGI_CHAR_FAILED) {
            fail_at(p, NG_ERR_PARSE, line, column, "carriage return not followed by a line feed");
            return char_error;
        }
    }
    if (c == NGI_CHAR_FAILED) {
        fail_at(p, p->source.status, p->source.line, p->source.column, "read error");
        return char_error;
    }
    return c;
}

static void advance(non_parser *p)
{
    ngi_char_source_advance(&p->source);
}

static int skip_spaces(non_parser *p)
{
    int c = peek(p);
    while (c == ' ') {
        advance(p);
        c = peek(p);
    }
    return c;
}

/* Consumes the end of a line that may only hold spaces after @p name. */
static ng_status expect_line_end(non_parser *p, const char *name)
{
    const int c = skip_spaces(p);
    if (c == char_error) {
        return p->status;
    }
    if (!is_line_end(c)) {
        return fail_at(p, NG_ERR_PARSE, p->source.line, p->source.column,
                       "unexpected text after '%s'", name);
    }
    if (c == '\n') {
        advance(p);
    }
    return NG_OK;
}

static void skip_comment(non_parser *p)
{
    int c = ngi_char_source_peek(&p->source);
    while (c >= 0 && c != '\n') {
        advance(p);
        c = ngi_char_source_peek(&p->source);
    }
    if (c == '\n') {
        advance(p);
    } else if (c == NGI_CHAR_FAILED) {
        peek(p); /* records the read failure */
    }
}

/* Reads the bytes of a token: letters only when @p letters_only, otherwise everything up to a
 * space, a comma or the end of the line. Returns the byte that follows. */
static int read_token(non_parser *p, token *t, bool letters_only)
{
    *t = (token){.line = p->source.line, .column = p->source.column};
    int c = peek(p);
    while (c >= 0 && (letters_only ? is_letter(c) : (c != ' ' && c != ',' && c != '\n'))) {
        if (t->size < sizeof t->raw - 1u) {
            t->raw[t->size++] = (char)c;
        } else {
            t->truncated = true;
        }
        advance(p);
        c = peek(p);
    }
    return c;
}

static bool token_is(const token *t, const char *word)
{
    return !t->truncated && t->size == strlen(word) && memcmp(t->raw, word, t->size) == 0;
}

/* Escaped text of a token, for messages. */
typedef struct token_text {
    char text[NGI_TOKEN_TEXT_SIZE];
} token_text;

static token_text text_of(const token *t)
{
    token_text out;
    ngi_escape(out.text, t->raw, t->size, t->truncated);
    return out;
}

/* Value of a token of decimal digits without leading zero ("0" itself is valid). Values above
 * UINT32_MAX saturate there: every caller rejects them. */
static bool token_number(const token *t, uint64_t *value)
{
    if (t->size == 0u || t->truncated || (t->raw[0] == '0' && t->size > 1u)) {
        return false;
    }
    uint64_t v = 0;
    for (size_t i = 0; i < t->size; ++i) {
        if (t->raw[i] < '0' || t->raw[i] > '9') {
            return false;
        }
        if (v <= UINT32_MAX) {
            v = (v * decimal_base) + (uint64_t)(t->raw[i] - '0');
        }
    }
    *value = v;
    return true;
}

/* --- directives ------------------------------------------------------------------------- */

static ng_status parse_dimension(non_parser *p, const char *name, ng_dim *out)
{
    const int c = peek(p);
    if (c == ' ') {
        skip_spaces(p); /* a read failure here is reported by read_token below */
    } else if (!is_line_end(c)) {
        return c == char_error ? p->status
                               : fail_at(p, NG_ERR_PARSE, p->source.line, p->source.column,
                                         "invalid value for '%s'", name);
    }
    token t;
    if (read_token(p, &t, false) == char_error) {
        return p->status;
    }
    uint64_t value = 0;
    if (!token_number(&t, &value)) {
        return fail_at(p, NG_ERR_PARSE, t.line, t.column, "invalid value for '%s': '%s'", name,
                       text_of(&t).text);
    }
    if (value < 1u || value > NG_DIM_MAX) {
        return fail_at(p, NG_ERR_PARSE, t.line, t.column,
                       "%s out of range [1, %" NG_PRI_DIM "]: %s", name, NG_DIM_MAX,
                       text_of(&t).text);
    }
    *out = (ng_dim)value;
    return expect_line_end(p, name);
}

static ng_status append_text(non_parser *p, ng_non_field field, const char *bytes, size_t size)
{
    if (p->metadata == nullptr) {
        return NG_OK;
    }
    const ng_status status = ngi_non_metadata_append(p->metadata, field, bytes, size);
    if (status != NG_OK) {
        return fail_at(p, status, p->source.line, p->source.column, "out of memory");
    }
    return NG_OK;
}

/* Content of a quoted value, up to and including the closing quote. */
static ng_status parse_quoted(non_parser *p, ng_non_field field)
{
    const char *name = field_names[field];
    const uint64_t line = p->source.line;
    const uint64_t column = p->source.column;
    char stage[text_stage_size];
    size_t staged = 0;
    size_t total = 0;
    for (int c = peek(p); c != '"'; c = peek(p)) {
        if (c == char_error) {
            return p->status;
        }
        if (is_line_end(c) || (c < first_printable && c != '\t')) {
            return fail_at(p, NG_ERR_PARSE, p->source.line, p->source.column,
                           is_line_end(c) ? "unterminated value for '%s'"
                                          : "control character in the value of '%s'",
                           name);
        }
        if (field != NG_NON_GOAL && total == text_value_max) {
            return fail_at(p, NG_ERR_PARSE, line, column, "'%s' longer than %zu bytes", name,
                           text_value_max);
        }
        stage[staged++] = (char)c;
        ++total;
        advance(p);
        if (staged == sizeof stage && append_text(p, field, stage, staged) != NG_OK) {
            return p->status;
        }
        staged = staged == sizeof stage ? 0u : staged;
    }
    advance(p); /* closing quote */
    return append_text(p, field, stage, staged);
}

static ng_status parse_text(non_parser *p, ng_non_field field)
{
    const char *name = field_names[field];
    int c = peek(p);
    if (c == ' ') {
        c = skip_spaces(p);
    }
    if (c == char_error) {
        return p->status;
    }
    if (c != '"') {
        return fail_at(p, NG_ERR_PARSE, p->source.line, p->source.column,
                       "expected a quoted value for '%s'", name);
    }
    advance(p);
    /* An empty value is present: create the field before any byte arrives. */
    if (append_text(p, field, nullptr, 0) != NG_OK || parse_quoted(p, field) != NG_OK) {
        return p->status;
    }
    return expect_line_end(p, name);
}

static ng_status parse_directive(non_parser *p, const token *word)
{
    const bool is_width = token_is(word, "width");
    if (is_width || token_is(word, "height")) {
        bool *seen = is_width ? &p->has_width : &p->has_height;
        if (*seen) {
            return fail_at(p, NG_ERR_PARSE, word->line, word->column, "duplicate directive '%s'",
                           word->raw);
        }
        *seen = true;
        return parse_dimension(p, word->raw, is_width ? &p->width : &p->height);
    }
    for (size_t f = 0; f < NG_NON_FIELD_COUNT; ++f) {
        if (token_is(word, field_names[f])) {
            if (p->seen[f]) {
                return fail_at(p, NG_ERR_PARSE, word->line, word->column,
                               "duplicate directive '%s'", field_names[f]);
            }
            p->seen[f] = true;
            return parse_text(p, (ng_non_field)f);
        }
    }
    return fail_at(p, NG_ERR_PARSE, word->line, word->column, "unknown directive '%s'",
                   text_of(word).text);
}

/* --- sections --------------------------------------------------------------------------- */

static ng_status start_rows(non_parser *p, const token *word)
{
    if (p->section != SECTION_HEADER) {
        return fail_at(p, NG_ERR_PARSE, word->line, word->column, "duplicate section 'rows'");
    }
    if (!p->has_width || !p->has_height) {
        return fail_at(p, NG_ERR_PARSE, word->line, word->column, "missing '%s' directive",
                       p->has_width ? "height" : "width");
    }
    if (expect_line_end(p, "rows") != NG_OK) {
        return p->status;
    }
    const uint64_t offset_count = (uint64_t)p->height + p->width + 1u;
    if (ckd_mul(&p->offsets_bytes, offset_count, sizeof(ng_run_index))) {
        return fail_at(p, NG_ERR_LIMIT, word->line, word->column, "puzzle too large");
    }
    p->offsets = p->allocator->alloc(p->allocator->ctx, p->offsets_bytes, alignof(ng_run_index));
    if (p->offsets == nullptr) {
        return fail_at(p, NG_ERR_OUT_OF_MEMORY, word->line, word->column, "out of memory");
    }
    p->offsets[0] = 0;
    p->section = SECTION_ROWS;
    p->remaining = p->height;
    return NG_OK;
}

static ng_status start_columns(non_parser *p, const token *word)
{
    if (p->section == SECTION_HEADER) {
        return fail_at(p, NG_ERR_PARSE, word->line, word->column,
                       "unexpected 'columns' before 'rows'");
    }
    if (p->section != SECTION_ROWS) {
        return fail_at(p, NG_ERR_PARSE, word->line, word->column, "duplicate section 'columns'");
    }
    if (expect_line_end(p, "columns") != NG_OK) {
        return p->status;
    }
    p->section = SECTION_COLUMNS;
    p->remaining = p->width;
    return NG_OK;
}

/* A line outside the clue sections: blank, a directive or a section keyword. */
static ng_status parse_keyword_line(non_parser *p)
{
    const int c = skip_spaces(p);
    if (c == char_error) {
        return p->status;
    }
    if (is_line_end(c)) {
        return expect_line_end(p, "blank line");
    }
    if (!is_letter(c)) {
        const bool after_clues = p->section == SECTION_ROWS || p->section == SECTION_TRAILER;
        if (after_clues && c >= '0' && c <= '9') {
            const bool rows = p->section == SECTION_ROWS;
            return fail_at(p, NG_ERR_PARSE, p->source.line, 1,
                           "expected %" NG_PRI_DIM " %s clues, got more",
                           rows ? p->height : p->width, rows ? "row" : "column");
        }
        return fail_at(p, NG_ERR_PARSE, p->source.line, p->source.column, "expected a directive");
    }
    token word;
    if (read_token(p, &word, true) == char_error) {
        return p->status;
    }
    if (token_is(&word, "rows")) {
        return start_rows(p, &word);
    }
    if (token_is(&word, "columns")) {
        return start_columns(p, &word);
    }
    if (p->section == SECTION_ROWS) {
        return fail_at(p, NG_ERR_PARSE, word.line, word.column,
                       "expected 'columns' after the row clues");
    }
    return parse_directive(p, &word);
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

static ng_status push_run(non_parser *p, ng_dim run)
{
    if (p->run_count == p->run_capacity) {
        size_t capacity = p->run_capacity == 0u ? initial_run_capacity : p->run_capacity;
        size_t bytes = 0;
        if ((p->run_capacity != 0u && ckd_mul(&capacity, capacity, (size_t)2)) ||
            ckd_mul(&bytes, capacity, sizeof(ng_dim))) {
            return fail_at(p, NG_ERR_LIMIT, p->source.line, 1, "puzzle too large");
        }
        ng_dim *runs = p->allocator->alloc(p->allocator->ctx, bytes, alignof(ng_dim));
        if (runs == nullptr) {
            return fail_at(p, NG_ERR_OUT_OF_MEMORY, p->source.line, 1, "out of memory");
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

static ng_status add_run(non_parser *p, clue_line *cl, ng_dim run)
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
static ng_status parse_clue(non_parser *p, clue_line *cl, bool *done)
{
    token t;
    int c = read_token(p, &t, false);
    if (c == char_error) {
        return p->status;
    }
    if (t.size == 0u) {
        return fail_at(p, NG_ERR_PARSE, t.line, t.column,
                       c == ',' ? "invalid clue ',' at position %" PRIu64
                                : "missing clue at position %" PRIu64,
                       cl->position);
    }
    uint64_t value = 0;
    const bool is_number = token_number(&t, &value);
    c = skip_spaces(p);
    if (c == char_error) {
        return p->status;
    }
    *done = is_line_end(c);
    const bool lone_zero = is_number && value == 0u && cl->position == 1u && *done;
    if (!lone_zero && (!is_number || value == 0u || value > NG_DIM_MAX)) {
        return fail_at(p, NG_ERR_PARSE, t.line, t.column, "invalid clue '%s' at position %" PRIu64,
                       text_of(&t).text, cl->position);
    }
    if (!lone_zero && add_run(p, cl, (ng_dim)value) != NG_OK) {
        return p->status;
    }
    if (c == ',') {
        advance(p);
        skip_spaces(p);
    }
    ++cl->position;
    return p->status;
}

static ng_status check_line_fits(non_parser *p, const clue_line *cl)
{
    if (!cl->exceeded) {
        return NG_OK;
    }
    const ng_line_id index = cl->is_row ? p->next_line : p->next_line - p->height;
    return fail_at(p, NG_ERR_INCONSISTENT, cl->line, 1,
                   "%s %" NG_PRI_LINE_ID " clues need %" NG_PRI_CELL_INDEX
                   " cells, %s is %" NG_PRI_DIM,
                   cl->is_row ? "row" : "column", index + 1u, cl->need,
                   cl->is_row ? "width" : "height", cl->length);
}

static ng_status parse_clue_line(non_parser *p)
{
    const bool is_row = p->section == SECTION_ROWS;
    const char *kind = is_row ? "row" : "column";
    const ng_dim expected = is_row ? p->height : p->width;
    const uint64_t line = p->source.line;
    const int c = skip_spaces(p);
    if (c == char_error) {
        return p->status;
    }
    if (is_line_end(c)) {
        return fail_at(p, NG_ERR_PARSE, line, 1,
                       "empty line in '%ss' section (write 0 for a line without runs)", kind);
    }
    if (is_letter(c)) {
        return fail_at(p, NG_ERR_PARSE, line, 1,
                       "expected %" NG_PRI_DIM " %s clues, got %" NG_PRI_DIM, expected, kind,
                       expected - p->remaining);
    }
    clue_line cl = {
        .line = line, .length = is_row ? p->width : p->height, .is_row = is_row, .position = 1};
    p->offsets[p->next_line] = p->run_count;
    for (bool done = false; !done;) {
        if (parse_clue(p, &cl, &done) != NG_OK) {
            return p->status;
        }
    }
    if (check_line_fits(p, &cl) != NG_OK || expect_line_end(p, "clues") != NG_OK) {
        return p->status;
    }
    p->offsets[++p->next_line] = p->run_count;
    if (--p->remaining == 0u && !is_row) {
        p->section = SECTION_TRAILER;
    }
    return NG_OK;
}

/* --- whole file ------------------------------------------------------------------------- */

static ng_status check_complete(non_parser *p)
{
    const uint64_t line = p->source.line;
    const uint64_t column = p->source.column;
    switch (p->section) {
    case SECTION_HEADER:
        return fail_at(p, NG_ERR_PARSE, line, column,
                       !p->has_width    ? "missing 'width' directive"
                       : !p->has_height ? "missing 'height' directive"
                                        : "missing 'rows' section");
    case SECTION_ROWS:
        if (p->remaining != 0u) {
            return fail_at(p, NG_ERR_PARSE, line, column,
                           "expected %" NG_PRI_DIM " row clues, got %" NG_PRI_DIM, p->height,
                           p->height - p->remaining);
        }
        return fail_at(p, NG_ERR_PARSE, line, column, "missing 'columns' section");
    case SECTION_COLUMNS:
        return fail_at(p, NG_ERR_PARSE, line, column,
                       "expected %" NG_PRI_DIM " column clues, got %" NG_PRI_DIM, p->width,
                       p->width - p->remaining);
    case SECTION_TRAILER:
        break;
    }
    if (p->totals[0] != p->totals[1]) {
        return fail_at(p, NG_ERR_INCONSISTENT, line, column,
                       "row clues sum to %" NG_PRI_CELL_INDEX
                       ", column clues to %" NG_PRI_CELL_INDEX,
                       p->totals[0], p->totals[1]);
    }
    return NG_OK;
}

static ng_status parse_lines(non_parser *p)
{
    if (ngi_char_source_peek(&p->source) == utf8_bom_first) {
        return fail_at(p, NG_ERR_PARSE, 1, 1, "byte order mark not allowed (UTF-8 without BOM)");
    }
    while (p->status == NG_OK) {
        const int c = peek(p);
        if (c == char_error) {
            break;
        }
        if (c == NGI_CHAR_END) {
            return check_complete(p);
        }
        if (c == '#') {
            skip_comment(p);
        } else if ((p->section == SECTION_ROWS || p->section == SECTION_COLUMNS) &&
                   p->remaining != 0u) {
            parse_clue_line(p);
        } else {
            parse_keyword_line(p);
        }
    }
    return p->status;
}

static ng_status build_puzzle(non_parser *p, ng_puzzle **out)
{
    const ng_status status =
        ng_puzzle_create(p->allocator, p->width, p->height, p->runs, p->offsets, out);
    if (status != NG_OK) {
        return fail_at(p, status, p->source.line, p->source.column, "%s", ng_status_str(status));
    }
    return NG_OK;
}

static void release_parser(non_parser *p)
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
    non_parser p = {.allocator = allocator, .error = error, .status = NG_OK};
    if (ngi_char_source_init(&p.source, allocator, reader) != NG_OK) {
        return fail_at(&p, NG_ERR_OUT_OF_MEMORY, 1, 1, "out of memory");
    }
    if (metadata != nullptr && ngi_non_metadata_create(allocator, &p.metadata) != NG_OK) {
        fail_at(&p, NG_ERR_OUT_OF_MEMORY, 1, 1, "out of memory");
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
