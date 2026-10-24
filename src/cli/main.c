#include <stdio.h>
#include <string.h>

#include "nonogram/version.h"

/* Stable exit codes (REQUIREMENTS.md F-41). */
enum cli_exit_code : int {
    CLI_EXIT_SUCCESS = 0,
    CLI_EXIT_USAGE = 2,
    CLI_EXIT_INTERNAL = 4,
};

static int print_usage(FILE *out)
{
    static const char usage[] = "usage: nonogram --version\n";
    if (fputs(usage, out) == EOF) {
        return CLI_EXIT_INTERNAL;
    }
    return CLI_EXIT_USAGE;
}

/* The C standard fixes main's signature: argv cannot be made const. */
/* cppcheck-suppress constParameter */
int main(int argc, char *argv[])
{
    if (argc == 2 && strcmp(argv[1], "--version") == 0) {
        if (printf("nonogram %s\n", ng_version_string()) < 0) {
            return CLI_EXIT_INTERNAL;
        }
        return CLI_EXIT_SUCCESS;
    }
    return print_usage(stderr);
}
