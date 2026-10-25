/**
 * @file non_parser.h
 * @brief State and shared helpers of the streaming .non reader (ngi, internal to src/io).
 *
 * The reader is split in three files: non_lexer.c (characters, positions, tokens),
 * non_directives.c (title, author, width, height, ...) and non_parser.c (sections, clue lines,
 * whole file).
 */
#ifndef NG_IO_INTERNAL_NON_PARSER_H
#define NG_IO_INTERNAL_NON_PARSER_H

#include <stddef.h>
#include <stdint.h>

#include "nonogram/allocator.h"
#include "nonogram/io.h"
#include "nonogram/status.h"
#include "nonogram/types.h"

#include "io/internal/char_source.h"
#include "io/internal/text.h"

/** @brief ngi_non_peek() result once a failure has been recorded in the parser. */
constexpr int NGI_NON_CHAR_ERROR = -3;

typedef enum ngi_non_section : uint8_t {
    NGI_NON_SECTION_HEADER,  /* directives before 'rows' */
    NGI_NON_SECTION_ROWS,    /* row clues, then blank lines or comments until 'columns' */
    NGI_NON_SECTION_COLUMNS, /* column clues */
    NGI_NON_SECTION_TRAILER, /* directives after the clues */
} ngi_non_section;

typedef struct ngi_non_parser {
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
    ngi_non_section section;
    ng_line_id next_line; /* unified id of the next clue line */
    ng_line_id remaining; /* clue lines still expected in the current section */
    ng_run_index *offsets;
    size_t offsets_bytes;
    ng_dim *runs;
    ng_run_index run_count;
    size_t run_capacity;
    ng_cell_index totals[2]; /* filled cells announced by the rows, by the columns */
} ngi_non_parser;

/* The first bytes of a token, for numbers and messages, and where it starts. */
typedef struct ngi_non_token {
    char raw[NGI_TOKEN_TEXT_SIZE];
    size_t size;
    bool truncated;
    uint64_t line;
    uint64_t column;
} ngi_non_token;

/* Escaped text of a token, for messages. */
typedef struct ngi_non_token_text {
    char text[NGI_TOKEN_TEXT_SIZE];
} ngi_non_token_text;

/** @brief Record the first failure only (later ones are consequences); returns p->status. */
[[gnu::format(printf, 5, 6)]] ng_status ngi_non_fail_at(ngi_non_parser *p, ng_status status,
                                                        uint64_t line, uint64_t column,
                                                        const char *format, ...);

bool ngi_non_is_letter(int c);

bool ngi_non_is_line_end(int c);

/** @brief Current byte with "\r\n" seen as '\n'; NGI_NON_CHAR_ERROR after a recorded read
 *  failure or lone carriage return. */
int ngi_non_peek(ngi_non_parser *p);

void ngi_non_advance(ngi_non_parser *p);

/** @brief Skip spaces; returns the byte that follows (as ngi_non_peek()). */
int ngi_non_skip_spaces(ngi_non_parser *p);

/** @brief Consume the end of a line that may only hold spaces after @p name. */
ng_status ngi_non_expect_line_end(ngi_non_parser *p, const char *name);

/** @brief Skip a comment line, up to and including its line feed. */
void ngi_non_skip_comment(ngi_non_parser *p);

/** @brief Read a token (letters only, or up to a space, comma or line end); returns the byte
 *  that follows. */
int ngi_non_read_token(ngi_non_parser *p, ngi_non_token *t, bool letters_only);

bool ngi_non_token_is(const ngi_non_token *t, const char *word);

/** @brief Escaped text of a token, for messages. */
ngi_non_token_text ngi_non_text_of(const ngi_non_token *t);

/** @brief Value of a decimal token without leading zero ("0" is valid); false otherwise. */
bool ngi_non_token_number(const ngi_non_token *t, uint64_t *value);

/** @brief Parse the rest of a directive line whose keyword is @p word (non_directives.c). */
ng_status ngi_non_parse_directive(ngi_non_parser *p, const ngi_non_token *word);

#endif /* NG_IO_INTERNAL_NON_PARSER_H */
