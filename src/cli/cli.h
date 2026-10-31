/**
 * @file cli.h
 * @brief Shared definitions of the nonogram command line tool (internal to src/cli).
 */
#ifndef NG_CLI_CLI_H
#define NG_CLI_CLI_H

#include <stdint.h>
#include <stdio.h>

#include "nonogram/grid.h"
#include "nonogram/puzzle.h"
#include "nonogram/solver.h"
#include "nonogram/types.h"

/* Grids wider or taller than this are summarised, not drawn (ROADMAP.md, M2). */
static constexpr ng_dim CLI_DEFAULT_MAX_RENDER = 200;

/* Stable exit codes (REQUIREMENTS.md F-41). */
enum cli_exit_code : int {
    CLI_EXIT_SUCCESS = 0,
    CLI_EXIT_FAILURE = 1, /* unsolvable puzzle, invalid or incomplete solution */
    CLI_EXIT_USAGE = 2,
    CLI_EXIT_INPUT = 3,    /* unreadable or malformed input, failed output */
    CLI_EXIT_INTERNAL = 4, /* library bug or resource exhaustion */
};

/** @brief Print the help text to @p out and flush it; false on a write failure. */
bool cli_print_usage(FILE *out);

/**
 * @brief Report a usage error on @p err: "nonogram: <message> '<argument>'" (argument may be
 *        nullptr), then the help text. The caller exits with CLI_EXIT_USAGE.
 */
void cli_report_usage_error(FILE *err, const char *message, const char *argument);

/** @brief Report a usage error (cli_report_usage_error) and return CLI_EXIT_USAGE. */
int cli_usage_error(FILE *err, const char *message, const char *argument);

/** @brief Outcome of matching an option that takes a value. */
typedef enum cli_option_match : uint8_t {
    CLI_OPTION_ABSENT,  /* the argument is not this option */
    CLI_OPTION_PRESENT, /* *value is set */
    CLI_OPTION_MISSING, /* the option ends the command line without its value */
} cli_option_match;

/**
 * @brief Match "--name value" or "--name=value" at argv[*index], advancing *index past a
 *        separate value. A value that starts with "--" counts as missing.
 */
cli_option_match cli_match_option(int argc, char **argv, int *index, const char *name,
                                  const char **value);

/**
 * @brief Parse a decimal count in [1, @p max]: digits only, no sign, no space.
 * @return true with *out set; false for anything else, including 0 (*out unchanged).
 */
bool cli_parse_count(const char *text, uint64_t max, uint64_t *out);

/**
 * @brief Read a .non file, reporting any failure on @p err.
 * @return CLI_EXIT_SUCCESS with *puzzle set, or the exit code of the failure.
 */
int cli_load_puzzle(FILE *err, const char *path, ng_puzzle **puzzle);

/**
 * @brief Read a .sol file whose dimensions must match @p puzzle, reporting failures on @p err.
 * @return CLI_EXIT_SUCCESS with *grid set, or the exit code of the failure (*grid may then
 *         be set and must still be destroyed).
 */
int cli_load_grid(FILE *err, const char *path, const ng_puzzle *puzzle, ng_grid **grid);

/**
 * @brief Run "nonogram verify".
 * @param argc, argv Arguments after "verify".
 * @return An exit code.
 */
int cli_verify(int argc, char **argv, FILE *out, FILE *err);

/**
 * @brief Run "nonogram solve".
 * @param argc, argv Arguments after "solve".
 * @return An exit code.
 */
int cli_solve(int argc, char **argv, FILE *out, FILE *err);

/**
 * @brief Run "nonogram solve" with a given solver instead of the registry's choice.
 *
 * Used by cli_solve() with @p solver = nullptr (--solver or the default solver), and by the
 * tests with a fake solver, to exercise what follows the solve (F-32 check, outputs).
 * @return An exit code.
 */
int cli_solve_with(int argc, char **argv, FILE *out, FILE *err, const ng_solver_vtable *solver);

/**
 * @brief Run "nonogram list-solvers".
 * @param argc, argv Arguments after "list-solvers" (none is accepted).
 * @return An exit code.
 */
int cli_list_solvers(int argc, char **argv, FILE *out, FILE *err);

/**
 * @brief Run "nonogram show".
 * @param argc, argv Arguments after "show".
 * @return An exit code.
 */
int cli_show(int argc, char **argv, FILE *out, FILE *err);

#endif /* NG_CLI_CLI_H */
