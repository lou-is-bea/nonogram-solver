/**
 * @file cli.h
 * @brief Shared definitions of the nonogram command line tool (internal to src/cli).
 */
#ifndef NG_CLI_CLI_H
#define NG_CLI_CLI_H

#include <stdio.h>

#include "nonogram/grid.h"
#include "nonogram/puzzle.h"

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
 * @brief Run "nonogram show".
 * @param argc, argv Arguments after "show".
 * @return An exit code.
 */
int cli_show(int argc, char **argv, FILE *out, FILE *err);

#endif /* NG_CLI_CLI_H */
