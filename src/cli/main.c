#include <stdio.h>
#include <string.h>

#include "nonogram/version.h"

#include "cli.h"

static const char usage_text[] =
    "usage: nonogram <command> [arguments]\n"
    "\n"
    "commands:\n"
    "  show <puzzle.non> [--solution <grid.sol>] [--unicode] [--max-render <n>]\n"
    "      Print the puzzle with its clues, and a solution or partial grid if given.\n"
    "      Grids wider or taller than <n> cells (default 200) are summarised.\n"
    "\n"
    "options:\n"
    "  --help       print this help\n"
    "  --version    print the version\n";

void cli_print_usage(FILE *out)
{
    /* A failure to print the help is not worth a different exit code. */
    (void)fputs(usage_text, out);
}

void cli_report_usage_error(FILE *err, const char *message, const char *argument)
{
    if (argument != nullptr) {
        (void)fprintf(err, "nonogram: %s '%s'\n\n", message, argument);
    } else {
        (void)fprintf(err, "nonogram: %s\n\n", message);
    }
    cli_print_usage(err);
}

/* The C standard fixes main's signature: argv cannot be made const. */
/* cppcheck-suppress constParameter */
int main(int argc, char *argv[])
{
    if (argc < 2) {
        cli_report_usage_error(stderr, "missing command", nullptr);
        return CLI_EXIT_USAGE;
    }
    const char *command = argv[1];
    if (strcmp(command, "--version") == 0 && argc == 2) {
        return printf("nonogram %s\n", ng_version_string()) < 0 ? CLI_EXIT_INPUT : CLI_EXIT_SUCCESS;
    }
    if ((strcmp(command, "--help") == 0 || strcmp(command, "-h") == 0) && argc == 2) {
        cli_print_usage(stdout);
        return CLI_EXIT_SUCCESS;
    }
    if (strcmp(command, "show") == 0) {
        return cli_show(argc - 2, argv + 2, stdout, stderr);
    }
    cli_report_usage_error(stderr, "unknown command", command);
    return CLI_EXIT_USAGE;
}
