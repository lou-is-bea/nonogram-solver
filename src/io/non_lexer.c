#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "nonogram/allocator.h"
#include "nonogram/io.h"
#include "nonogram/status.h"
#include "nonogram/types.h"

#include "io/internal/char_source.h"
#include "io/internal/non_parser.h"
#include "io/internal/text.h"

/* Characters, positions and tokens of the .non reader (see io/internal/non_parser.h). */

static constexpr uint64_t decimal_base = 10;

/* Records the first failure only: what follows it is a consequence. */
[[gnu::format(printf, 5, 6)]] ng_status ngi_non_fail_at(ngi_non_parser *p, ng_status status,
                                                        uint64_t line, uint64_t column,
                                                        const char *format, ...)
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

bool ngi_non_is_letter(int c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

bool ngi_non_is_line_end(int c)
{
    return c == '\n' || c == NGI_CHAR_END;
}

/* Current byte, with "\r\n" seen as '\n'. Returns NGI_NON_CHAR_ERROR once a read failure or a lone
 * carriage return has been recorded. */
int ngi_non_peek(ngi_non_parser *p)
{
    int c = ngi_char_source_peek(&p->source);
    if (c == '\r') {
        const uint64_t line = p->source.line;
        const uint64_t column = p->source.column;
        ngi_char_source_advance(&p->source);
        c = ngi_char_source_peek(&p->source);
        if (c != '\n' && c != NGI_CHAR_FAILED) {
            ngi_non_fail_at(p, NG_ERR_PARSE, line, column,
                            "carriage return not followed by a line feed");
            return NGI_NON_CHAR_ERROR;
        }
    }
    if (c == NGI_CHAR_FAILED) {
        ngi_non_fail_at(p, p->source.status, p->source.line, p->source.column, "read error");
        return NGI_NON_CHAR_ERROR;
    }
    return c;
}

void ngi_non_advance(ngi_non_parser *p)
{
    ngi_char_source_advance(&p->source);
}

int ngi_non_skip_spaces(ngi_non_parser *p)
{
    int c = ngi_non_peek(p);
    while (c == ' ') {
        ngi_non_advance(p);
        c = ngi_non_peek(p);
    }
    return c;
}

/* Consumes the end of a line that may only hold spaces after @p name. */
ng_status ngi_non_expect_line_end(ngi_non_parser *p, const char *name)
{
    const int c = ngi_non_skip_spaces(p);
    if (c == NGI_NON_CHAR_ERROR) {
        return p->status;
    }
    if (!ngi_non_is_line_end(c)) {
        return ngi_non_fail_at(p, NG_ERR_PARSE, p->source.line, p->source.column,
                               "unexpected text after '%s'", name);
    }
    if (c == '\n') {
        ngi_non_advance(p);
    }
    return NG_OK;
}

void ngi_non_skip_comment(ngi_non_parser *p)
{
    /* ngi_non_peek() also applies here: a carriage return not followed by a line feed is
     * refused inside a comment as anywhere else (FILE_FORMATS.md §1.3). */
    int c = ngi_non_peek(p);
    while (c >= 0 && c != '\n') {
        ngi_non_advance(p);
        c = ngi_non_peek(p);
    }
    if (c == '\n') {
        ngi_non_advance(p);
    }
}

/* Reads the bytes of a token: letters only when @p letters_only, otherwise everything up to a
 * space, a comma or the end of the line. Returns the byte that follows. */
int ngi_non_read_token(ngi_non_parser *p, ngi_non_token *t, bool letters_only)
{
    *t = (ngi_non_token){.line = p->source.line, .column = p->source.column};
    int c = ngi_non_peek(p);
    while (c >= 0 && (letters_only ? ngi_non_is_letter(c) : (c != ' ' && c != ',' && c != '\n'))) {
        if (t->size < sizeof t->raw - 1u) {
            t->raw[t->size++] = (char)c;
        } else {
            t->truncated = true;
        }
        ngi_non_advance(p);
        c = ngi_non_peek(p);
    }
    return c;
}

bool ngi_non_token_is(const ngi_non_token *t, const char *word)
{
    return !t->truncated && t->size == strlen(word) && memcmp(t->raw, word, t->size) == 0;
}

ngi_non_token_text ngi_non_text_of(const ngi_non_token *t)
{
    ngi_non_token_text out;
    ngi_escape(out.text, t->raw, t->size, t->truncated);
    return out;
}

/* Value of a token of decimal digits without leading zero ("0" itself is valid). Values above
 * UINT32_MAX saturate there: every caller rejects them. */
bool ngi_non_token_number(const ngi_non_token *t, uint64_t *value)
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
