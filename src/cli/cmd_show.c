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
/* Decimal value in [1, NG_DIM_MAX]; false for anything else. */
static bool parse_dim(const char *text, ng_dim *out)
{
    uint64_t value = 0;
    if (!cli_parse_count(text, NG_DIM_MAX, &value)) {
        return false;
    }
    *out = (ng_dim)value;
    return true;
}

typedef struct show_args {
    const char *puzzle_path;
    const char *solution_path;
    bool unicode;
    bool has_max_render;
    ng_dim max_render;
} show_args;

/* Handles one argument; returns CLI_EXIT_SUCCESS or a usage error. */
static int parse_arg(int argc, char **argv, int *index, show_args *args, FILE *err)
{
    const char *arg = argv[*index];
    const char *value = nullptr;
    if (strcmp(arg, "--unicode") == 0) {
        if (args->unicode) {
            return cli_usage_error(err, "repeated option", arg);
        }
        args->unicode = true;
        return CLI_EXIT_SUCCESS;
    }
    cli_option_match match = cli_match_option(argc, argv, index, "--solution", &value);
    if (match == CLI_OPTION_PRESENT) {
        if (args->solution_path != nullptr) {
            return cli_usage_error(err, "repeated option", "--solution");
        }
        args->solution_path = value;
        return CLI_EXIT_SUCCESS;
    }
    if (match == CLI_OPTION_ABSENT) {
        match = cli_match_option(argc, argv, index, "--max-render", &value);
        if (match == CLI_OPTION_PRESENT) {
            if (args->has_max_render) {
                return cli_usage_error(err, "repeated option", "--max-render");
            }
            args->has_max_render = true;
            if (parse_dim(value, &args->max_render)) {
                return CLI_EXIT_SUCCESS;
            }
            return cli_usage_error(err, "invalid --max-render value", value);
        }
    }
    if (match == CLI_OPTION_MISSING) {
        return cli_usage_error(err, "missing value for", arg);
    }
    if (arg[0] == '-') {
        return cli_usage_error(err, "unknown option", arg);
    }
    if (args->puzzle_path != nullptr) {
        return cli_usage_error(err, "unexpected argument", arg);
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
        return cli_usage_error(err, "show: missing puzzle file", nullptr);
    }
    return CLI_EXIT_SUCCESS;
}

static int print(FILE *out, FILE *err, const show_args *args, const ng_puzzle *puzzle,
                 const ng_grid *grid)
{
    const ng_writer w = ng_writer_from_file(out);
    const bool too_large =
        ng_puzzle_width(puzzle) > args->max_render || ng_puzzle_height(puzzle) > args->max_render;
    const ng_render_options options = {.unicode = args->unicode};
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
    code = cli_load_puzzle(err, args.puzzle_path, &puzzle);
    if (code == CLI_EXIT_SUCCESS && args.solution_path != nullptr) {
        code = cli_load_grid(err, args.solution_path, puzzle, &grid);
    }
    if (code == CLI_EXIT_SUCCESS) {
        code = print(out, err, &args, puzzle, grid);
    }
    ng_grid_destroy(grid);
    ng_puzzle_destroy(puzzle);
    return code;
}
