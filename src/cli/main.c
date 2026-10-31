#include <stdio.h>
#include <string.h>

#include "nonogram/version.h"

#include "cli.h"

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
        /* stdout is buffered: a write error may only show when it is flushed. */
        if (printf("nonogram %s\n", ng_version_string()) < 0 || fflush(stdout) == EOF) {
            return CLI_EXIT_INPUT;
        }
        return CLI_EXIT_SUCCESS;
    }
    if ((strcmp(command, "--help") == 0 || strcmp(command, "-h") == 0) && argc == 2) {
        /* stdout is buffered: a write error may only show when it is flushed. */
        return cli_print_usage(stdout) ? CLI_EXIT_SUCCESS : CLI_EXIT_INPUT;
    }
    if (strcmp(command, "show") == 0) {
        return cli_show(argc - 2, argv + 2, stdout, stderr);
    }
    if (strcmp(command, "verify") == 0) {
        return cli_verify(argc - 2, argv + 2, stdout, stderr);
    }
    if (strcmp(command, "solve") == 0) {
        return cli_solve(argc - 2, argv + 2, stdout, stderr);
    }
    if (strcmp(command, "list-solvers") == 0) {
        return cli_list_solvers(argc - 2, argv + 2, stdout, stderr);
    }
    cli_report_usage_error(stderr, "unknown command", command);
    return CLI_EXIT_USAGE;
}
