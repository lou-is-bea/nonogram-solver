#include <stdio.h>

#include "cli.h"

/* The help text of nonogram, shared by --help and the usage errors of every subcommand. */

static const char usage_text[] =
    "usage: nonogram <command> [arguments]\n"
    "\n"
    "commands:\n"
    "  show <puzzle.non> [--solution <grid.sol>] [--unicode] [--max-render <n>]\n"
    "      Print the puzzle with its clues, and a solution or partial grid if given.\n"
    "      Grids wider or taller than <n> cells (default 200) are summarised.\n"
    "  solve <puzzle.non> [--solver <name>] [--count <n>] [--max-nodes <n>]\n"
    "        [--max-time <ms>] [--max-memory <bytes>] [--output <grid.sol>] [--stats]\n"
    "        [--format text|json]\n"
    "      Solve the puzzle (default solver: the first complete one of list-solvers):\n"
    "      solved (exit 0); unsolvable, incomplete or limit reached (exit 1).\n"
    "  verify <puzzle.non> <grid.sol>\n"
    "      Check the grid against the clues: valid (exit 0), incomplete or invalid (exit 1).\n"
    "  list-solvers\n"
    "      List the solvers: name, complete or not, summary.\n"
    "\n"
    "options:\n"
    "  --help       print this help\n"
    "  --version    print the version\n";

bool cli_print_usage(FILE *out)
{
    return fputs(usage_text, out) != EOF && fflush(out) != EOF;
}

void cli_report_usage_error(FILE *err, const char *message, const char *argument)
{
    if (argument != nullptr) {
        (void)fprintf(err, "nonogram: %s '%s'\n\n", message, argument);
    } else {
        (void)fprintf(err, "nonogram: %s\n\n", message);
    }
    (void)cli_print_usage(err); /* already reporting an error: nothing more to say */
}
