#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "nonogram/allocator.h"
#include "nonogram/grid.h"
#include "nonogram/io.h"
#include "nonogram/puzzle.h"
#include "nonogram/render.h"
#include "nonogram/status.h"
#include "nonogram/stream.h"
#include "nonogram/types.h"

#include "cli.h"

/* nonogram show <puzzle.non> [--solution <grid.sol>] [--unicode] [--max-render <n>] */

static constexpr ng_dim default_max_render = 200; /* ROADMAP.md, M2 */
static constexpr uint64_t decimal_base = 10;

typedef struct show_args {
    const char *puzzle_path;
    const char *solution_path;
    bool unicode;
    ng_dim max_render;
} show_args;

/* Decimal value in [1, NG_DIM_MAX]; false for anything else. */
static bool parse_dim(const char *text, ng_dim *out)
{
    uint64_t value = 0;
    if (*text == '\0') {
        return false;
    }
    for (const char *c = text; *c != '\0'; ++c) {
        if (*c < '0' || *c > '9') {
            return false;
        }
        value = (value * decimal_base) + (uint64_t)(*c - '0');
        if (value > NG_DIM_MAX) {
            return false;
        }
    }
    *out = (ng_dim)value;
    return value != 0u;
}

static int usage_error(FILE *err, const char *message, const char *argument)
{
    cli_report_usage_error(err, message, argument);
    return CLI_EXIT_USAGE;
}

typedef enum option_match : uint8_t {
    OPTION_ABSENT,  /* the argument is not this option */
    OPTION_PRESENT, /* *value is set */
    OPTION_MISSING, /* the option ends the command line without its value */
} option_match;

/* Matches "--name value" or "--name=value" at argv[*index], advancing *index past a separate
 * value. */
static option_match match_option(int argc, char **argv, int *index, const char *name,
                                 const char **value)
{
    const char *arg = argv[*index];
    const size_t length = strlen(name);
    if (strncmp(arg, name, length) != 0 || (arg[length] != '=' && arg[length] != '\0')) {
        return OPTION_ABSENT;
    }
    if (arg[length] == '=') {
        *value = arg + length + 1;
        return OPTION_PRESENT;
    }
    if (*index + 1 >= argc || argv[*index + 1] == nullptr) {
        return OPTION_MISSING;
    }
    *value = argv[++*index];
    return OPTION_PRESENT;
}

/* Handles one argument; returns CLI_EXIT_SUCCESS or a usage error. */
static int parse_arg(int argc, char **argv, int *index, show_args *args, FILE *err)
{
    const char *arg = argv[*index];
    const char *value = nullptr;
    if (strcmp(arg, "--unicode") == 0) {
        args->unicode = true;
        return CLI_EXIT_SUCCESS;
    }
    option_match match = match_option(argc, argv, index, "--solution", &value);
    if (match == OPTION_PRESENT) {
        args->solution_path = value;
        return CLI_EXIT_SUCCESS;
    }
    if (match == OPTION_ABSENT) {
        match = match_option(argc, argv, index, "--max-render", &value);
        if (match == OPTION_PRESENT) {
            if (parse_dim(value, &args->max_render)) {
                return CLI_EXIT_SUCCESS;
            }
            return usage_error(err, "invalid --max-render value", value);
        }
    }
    if (match == OPTION_MISSING) {
        return usage_error(err, "missing value for", arg);
    }
    if (arg[0] == '-') {
        return usage_error(err, "unknown option", arg);
    }
    if (args->puzzle_path != nullptr) {
        return usage_error(err, "unexpected argument", arg);
    }
    args->puzzle_path = arg;
    return CLI_EXIT_SUCCESS;
}

static int parse_args(int argc, char **argv, show_args *args, FILE *err)
{
    *args = (show_args){.max_render = default_max_render};
    /* argv[argc] is the only null entry; the check keeps the static analyser informed. */
    for (int i = 0; i < argc && argv[i] != nullptr; ++i) {
        const int code = parse_arg(argc, argv, &i, args, err);
        if (code != CLI_EXIT_SUCCESS) {
            return code;
        }
    }
    if (args->puzzle_path == nullptr) {
        return usage_error(err, "show: missing puzzle file", nullptr);
    }
    return CLI_EXIT_SUCCESS;
}

/* Exit code for a library failure while reading @p path. */
static int report_read_error(FILE *err, const char *path, ng_status status,
                             const ng_parse_error *error)
{
    if (status == NG_ERR_OUT_OF_MEMORY || status == NG_ERR_INTERNAL ||
        status == NG_ERR_INVALID_ARGUMENT) {
        (void)fprintf(err, "nonogram: %s: %s\n", path, ng_status_str(status));
        return CLI_EXIT_INTERNAL;
    }
    (void)fprintf(err, "%s:%" PRIu64 ":%" PRIu64 ": error: %s\n", path, error->line, error->column,
                  error->message);
    return CLI_EXIT_INPUT;
}

static FILE *open_input(FILE *err, const char *path)
{
    FILE *file = fopen(path, "rb");
    if (file == nullptr) {
        (void)fprintf(err, "nonogram: cannot open '%s': %s\n", path, strerror(errno));
    }
    return file;
}

static int load_puzzle(FILE *err, const char *path, ng_puzzle **puzzle)
{
    FILE *file = open_input(err, path);
    if (file == nullptr) {
        return CLI_EXIT_INPUT;
    }
    const ng_reader r = ng_reader_from_file(file);
    ng_parse_error error = {};
    const ng_status status = ng_io_read_non(ng_allocator_default(), &r, puzzle, nullptr, &error);
    (void)fclose(file); /* read-only: nothing to lose */
    return status == NG_OK ? CLI_EXIT_SUCCESS : report_read_error(err, path, status, &error);
}

static int load_grid(FILE *err, const char *path, const ng_puzzle *puzzle, ng_grid **grid)
{
    FILE *file = open_input(err, path);
    if (file == nullptr) {
        return CLI_EXIT_INPUT;
    }
    const ng_reader r = ng_reader_from_file(file);
    ng_parse_error error = {};
    const ng_status status = ng_io_read_sol(ng_allocator_default(), &r, grid, &error);
    (void)fclose(file);
    if (status != NG_OK) {
        return report_read_error(err, path, status, &error);
    }
    if (ng_grid_width(*grid) != ng_puzzle_width(puzzle) ||
        ng_grid_height(*grid) != ng_puzzle_height(puzzle)) {
        (void)fprintf(err,
                      "nonogram: %s: grid is %" NG_PRI_DIM " x %" NG_PRI_DIM
                      ", puzzle is %" NG_PRI_DIM " x %" NG_PRI_DIM "\n",
                      path, ng_grid_width(*grid), ng_grid_height(*grid), ng_puzzle_width(puzzle),
                      ng_puzzle_height(puzzle));
        return CLI_EXIT_INPUT;
    }
    return CLI_EXIT_SUCCESS;
}

static int print(FILE *out, FILE *err, const show_args *args, const ng_puzzle *puzzle,
                 const ng_grid *grid)
{
    const ng_writer w = ng_writer_from_file(out);
    const bool too_large =
        ng_puzzle_width(puzzle) > args->max_render || ng_puzzle_height(puzzle) > args->max_render;
    const ng_render_options options = {.show_clues = true, .unicode = args->unicode};
    const ng_status status = too_large ? ng_render_summary(puzzle, grid, &w)
                                       : ng_render_text(puzzle, grid, &options, &w);
    if (status != NG_OK || fflush(out) != 0) {
        (void)fprintf(err, "nonogram: cannot write the output: %s\n",
                      status != NG_OK ? ng_status_str(status) : strerror(errno));
        return status == NG_OK || status == NG_ERR_IO ? CLI_EXIT_INPUT : CLI_EXIT_INTERNAL;
    }
    return CLI_EXIT_SUCCESS;
}

int cli_show(int argc, char **argv, FILE *out, FILE *err)
{
    show_args args;
    int code = parse_args(argc, argv, &args, err);
    if (code != CLI_EXIT_SUCCESS) {
        return code;
    }
    ng_puzzle *puzzle = nullptr;
    ng_grid *grid = nullptr;
    code = load_puzzle(err, args.puzzle_path, &puzzle);
    if (code == CLI_EXIT_SUCCESS && args.solution_path != nullptr) {
        code = load_grid(err, args.solution_path, puzzle, &grid);
    }
    if (code == CLI_EXIT_SUCCESS) {
        code = print(out, err, &args, puzzle, grid);
    }
    ng_grid_destroy(grid);
    ng_puzzle_destroy(puzzle);
    return code;
}
