#include <stdint.h>
#include <string.h>

#include "cli.h"

/* Command line parsing shared by the subcommands. */

static constexpr uint64_t decimal_base = 10;

bool cli_parse_count(const char *text, uint64_t max, uint64_t *out)
{
    uint64_t value = 0;
    if (*text == '\0') {
        return false;
    }
    for (const char *c = text; *c != '\0'; ++c) {
        if (*c < '0' || *c > '9') {
            return false;
        }
        const uint64_t digit = (uint64_t)(*c - '0');
        if (value > (max - digit) / decimal_base) {
            return false;
        }
        value = (value * decimal_base) + digit;
    }
    *out = value;
    return value != 0u;
}

cli_option_match cli_match_option(int argc, char **argv, int *index, const char *name,
                                  const char **value)
{
    const char *arg = argv[*index];
    const size_t length = strlen(name);
    if (strncmp(arg, name, length) != 0 || (arg[length] != '=' && arg[length] != '\0')) {
        return CLI_OPTION_ABSENT;
    }
    if (arg[length] == '=') {
        *value = arg + length + 1;
        return CLI_OPTION_PRESENT;
    }
    /* "--solution --unicode": an option where a value is expected means the value is missing. */
    if (*index + 1 >= argc || argv[*index + 1] == nullptr ||
        strncmp(argv[*index + 1], "--", 2) == 0) {
        return CLI_OPTION_MISSING;
    }
    *value = argv[++*index];
    return CLI_OPTION_PRESENT;
}

int cli_usage_error(FILE *err, const char *message, const char *argument)
{
    cli_report_usage_error(err, message, argument);
    return CLI_EXIT_USAGE;
}
