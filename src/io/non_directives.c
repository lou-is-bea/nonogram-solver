#include <stddef.h>
#include <stdint.h>

#include "nonogram/allocator.h"
#include "nonogram/io.h"
#include "nonogram/status.h"
#include "nonogram/types.h"

#include "io/internal/non_metadata.h"
#include "io/internal/non_parser.h"
#include "io/internal/text.h"

/* Directive lines of the .non reader: text values (title, author, ...) and dimensions. */

static constexpr size_t text_value_max = 255;  /* FILE_FORMATS.md §1.3 */
static constexpr size_t text_stage_size = 256; /* bytes of a value appended at once */
static constexpr int first_printable = 0x20;

static const char *const field_names[NG_NON_FIELD_COUNT] = {
    [NG_NON_TITLE] = "title",         [NG_NON_AUTHOR] = "author",
    [NG_NON_COPYRIGHT] = "copyright", [NG_NON_LICENSE] = "license",
    [NG_NON_CATALOGUE] = "catalogue", [NG_NON_GOAL] = "goal",
};

static ng_status parse_dimension(ngi_non_parser *p, const char *name, ng_dim *out)
{
    const int c = ngi_non_peek(p);
    if (c == ' ') {
        ngi_non_skip_spaces(p); /* a read failure here is reported by read_token below */
    } else if (!ngi_non_is_line_end(c)) {
        return c == NGI_NON_CHAR_ERROR
                   ? p->status
                   : ngi_non_fail_at(p, NG_ERR_PARSE, p->source.line, p->source.column,
                                     "invalid value for '%s'", name);
    }
    ngi_non_token t;
    if (ngi_non_read_token(p, &t, false) == NGI_NON_CHAR_ERROR) {
        return p->status;
    }
    uint64_t value = 0;
    if (!ngi_non_token_number(&t, &value)) {
        return ngi_non_fail_at(p, NG_ERR_PARSE, t.line, t.column, "invalid value for '%s': '%s'",
                               name, ngi_non_text_of(&t).text);
    }
    if (value < 1u || value > NG_DIM_MAX) {
        return ngi_non_fail_at(p, NG_ERR_PARSE, t.line, t.column,
                               "%s out of range [1, %" NG_PRI_DIM "]: %s", name, NG_DIM_MAX,
                               ngi_non_text_of(&t).text);
    }
    *out = (ng_dim)value;
    return ngi_non_expect_line_end(p, name);
}

static ng_status append_text(ngi_non_parser *p, ng_non_field field, const char *bytes, size_t size)
{
    if (p->metadata == nullptr) {
        return NG_OK;
    }
    const ng_status status = ngi_non_metadata_append(p->metadata, field, bytes, size);
    if (status != NG_OK) {
        return ngi_non_fail_at(p, status, p->source.line, p->source.column, "out of memory");
    }
    return NG_OK;
}

/* Content of a quoted value, up to and including the closing quote. */
static ng_status parse_quoted(ngi_non_parser *p, ng_non_field field)
{
    const char *name = field_names[field];
    const uint64_t line = p->source.line;
    const uint64_t column = p->source.column;
    char stage[text_stage_size];
    size_t staged = 0;
    size_t total = 0;
    for (int c = ngi_non_peek(p); c != '"'; c = ngi_non_peek(p)) {
        if (c == NGI_NON_CHAR_ERROR) {
            return p->status;
        }
        if (ngi_non_is_line_end(c) || (c < first_printable && c != '\t')) {
            return ngi_non_fail_at(p, NG_ERR_PARSE, p->source.line, p->source.column,
                                   ngi_non_is_line_end(c)
                                       ? "unterminated value for '%s'"
                                       : "control character in the value of '%s'",
                                   name);
        }
        if (field != NG_NON_GOAL && total == text_value_max) {
            return ngi_non_fail_at(p, NG_ERR_PARSE, line, column, "'%s' longer than %zu bytes",
                                   name, text_value_max);
        }
        stage[staged++] = (char)c;
        ++total;
        ngi_non_advance(p);
        if (staged == sizeof stage && append_text(p, field, stage, staged) != NG_OK) {
            return p->status;
        }
        staged = staged == sizeof stage ? 0u : staged;
    }
    ngi_non_advance(p); /* closing quote */
    return append_text(p, field, stage, staged);
}

static ng_status parse_text(ngi_non_parser *p, ng_non_field field)
{
    const char *name = field_names[field];
    int c = ngi_non_peek(p);
    if (c == ' ') {
        c = ngi_non_skip_spaces(p);
    }
    if (c == NGI_NON_CHAR_ERROR) {
        return p->status;
    }
    if (c != '"') {
        return ngi_non_fail_at(p, NG_ERR_PARSE, p->source.line, p->source.column,
                               "expected a quoted value for '%s'", name);
    }
    ngi_non_advance(p);
    /* An empty value is present: create the field before any byte arrives. */
    if (append_text(p, field, nullptr, 0) != NG_OK || parse_quoted(p, field) != NG_OK) {
        return p->status;
    }
    return ngi_non_expect_line_end(p, name);
}

ng_status ngi_non_parse_directive(ngi_non_parser *p, const ngi_non_token *word)
{
    const bool is_width = ngi_non_token_is(word, "width");
    if (is_width || ngi_non_token_is(word, "height")) {
        bool *seen = is_width ? &p->has_width : &p->has_height;
        if (*seen) {
            return ngi_non_fail_at(p, NG_ERR_PARSE, word->line, word->column,
                                   "duplicate directive '%s'", word->raw);
        }
        *seen = true;
        return parse_dimension(p, word->raw, is_width ? &p->width : &p->height);
    }
    for (size_t f = 0; f < NG_NON_FIELD_COUNT; ++f) {
        if (ngi_non_token_is(word, field_names[f])) {
            if (p->seen[f]) {
                return ngi_non_fail_at(p, NG_ERR_PARSE, word->line, word->column,
                                       "duplicate directive '%s'", field_names[f]);
            }
            p->seen[f] = true;
            return parse_text(p, (ng_non_field)f);
        }
    }
    return ngi_non_fail_at(p, NG_ERR_PARSE, word->line, word->column, "unknown directive '%s'",
                           ngi_non_text_of(word).text);
}
