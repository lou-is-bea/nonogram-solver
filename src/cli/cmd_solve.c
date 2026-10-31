#include <errno.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "nonogram/allocator.h"
#include "nonogram/grid.h"
#include "nonogram/io.h"
#include "nonogram/puzzle.h"
#include "nonogram/render.h"
#include "nonogram/solver.h"
#include "nonogram/status.h"
#include "nonogram/stream.h"
#include "nonogram/types.h"
#include "nonogram/verify.h"

#include "cli.h"

/* nonogram solve <puzzle.non> [--solver <name>] [--count <n>] [--max-nodes <n>]
 *                [--max-time <ms>] [--max-memory <bytes>] [--output <grid.sol>] [--stats]
 *                [--format text|json]
 * Exit codes (F-41): 0 solved; 1 unsolvable, incomplete or limit reached; 2 usage, including
 * a puzzle the solver cannot take within its limits (NG_ERR_LIMIT); 3 input or output; 4
 * internal. */

/* The options that take a value, in the order of the help text. */
enum solve_option : uint8_t {
    OPT_SOLVER,
    OPT_COUNT,
    OPT_MAX_NODES,
    OPT_MAX_TIME,
    OPT_MAX_MEMORY,
    OPT_OUTPUT,
    OPT_FORMAT,
    OPT_VALUE_COUNT,
};

static const char *const option_names[OPT_VALUE_COUNT] = {
    "--solver", "--count", "--max-nodes", "--max-time", "--max-memory", "--output", "--format",
};

typedef struct solve_args {
    const char *puzzle_path;
    const char *values[OPT_VALUE_COUNT]; /* nullptr: option absent */
    bool stats;
    bool json;
    ng_solve_options options;
} solve_args;

/* Handles one argument; returns CLI_EXIT_SUCCESS or a usage error. */
static int parse_arg(int argc, char **argv, int *index, solve_args *args, FILE *err)
{
    const char *arg = argv[*index];
    if (strcmp(arg, "--stats") == 0) {
        if (args->stats) {
            return cli_usage_error(err, "repeated option", arg);
        }
        args->stats = true;
        return CLI_EXIT_SUCCESS;
    }
    for (size_t i = 0; i < OPT_VALUE_COUNT; ++i) {
        const char *value = nullptr;
        const cli_option_match match = cli_match_option(argc, argv, index, option_names[i], &value);
        if (match == CLI_OPTION_MISSING) {
            return cli_usage_error(err, "missing value for", arg);
        }
        if (match == CLI_OPTION_PRESENT) {
            if (args->values[i] != nullptr) {
                return cli_usage_error(err, "repeated option", option_names[i]);
            }
            args->values[i] = value;
            return CLI_EXIT_SUCCESS;
        }
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

/* Converts the numeric options and --format; returns CLI_EXIT_SUCCESS or a usage error. */
static int convert_values(solve_args *args, FILE *err)
{
    static const uint64_t maxima[OPT_VALUE_COUNT] = {
        [OPT_COUNT] = UINT32_MAX,
        [OPT_MAX_NODES] = UINT64_MAX,
        [OPT_MAX_TIME] = UINT64_MAX,
        [OPT_MAX_MEMORY] = SIZE_MAX,
    };
    uint64_t numbers[OPT_VALUE_COUNT] = {[OPT_COUNT] = 1};
    for (size_t i = 0; i < OPT_VALUE_COUNT; ++i) {
        if (maxima[i] != 0u && args->values[i] != nullptr &&
            !cli_parse_count(args->values[i], maxima[i], &numbers[i])) {
            char message[sizeof "invalid --max-memory value"];
            (void)snprintf(message, sizeof message, "invalid %s value", option_names[i]);
            return cli_usage_error(err, message, args->values[i]);
        }
    }
    args->options.max_solutions = (uint32_t)numbers[OPT_COUNT];
    args->options.max_nodes = numbers[OPT_MAX_NODES];
    args->options.max_time_ms = numbers[OPT_MAX_TIME];
    args->options.max_memory_bytes = (size_t)numbers[OPT_MAX_MEMORY];
    const char *format = args->values[OPT_FORMAT];
    if (format != nullptr && strcmp(format, "text") != 0 && strcmp(format, "json") != 0) {
        return cli_usage_error(err, "invalid --format value", format);
    }
    args->json = format != nullptr && strcmp(format, "json") == 0;
    return CLI_EXIT_SUCCESS;
}

static int parse_args(int argc, char **argv, solve_args *args, FILE *err)
{
    *args = (solve_args){.options = ng_solve_options_default()};
    /* argv[argc] is the only null entry; the check keeps the static analyser informed. */
    for (int i = 0; i < argc && argv[i] != nullptr; ++i) {
        const int code = parse_arg(argc, argv, &i, args, err);
        if (code != CLI_EXIT_SUCCESS) {
            return code;
        }
    }
    if (args->puzzle_path == nullptr) {
        return cli_usage_error(err, "solve: missing puzzle file", nullptr);
    }
    return convert_values(args, err);
}

/* What the run produced, for the outputs. */
typedef struct solve_run {
    const solve_args *args;
    const char *solver_name;
    const ng_puzzle *puzzle;
    const ng_grid *grid;
    ng_solve_outcome outcome;
} solve_run;

/* The count is exact only when the search was conclusive (SOLVED or UNSOLVABLE) and ended
 * before the cap (F-35); after LIMIT_REACHED or INCOMPLETE it is a lower bound. */
static bool count_is_exact(const solve_run *run)
{
    const ng_solve_outcome *o = &run->outcome;
    const bool conclusive = o->result == NG_SOLVE_SOLVED || o->result == NG_SOLVE_UNSOLVABLE;
    return conclusive && o->stats.solutions_found < run->args->options.max_solutions;
}

static void print_solution_count(FILE *out, const solve_run *run)
{
    (void)fprintf(out, "solutions: %s%" PRIu32 "\n", count_is_exact(run) ? "" : "at least ",
                  run->outcome.stats.solutions_found);
}

static void print_stats_text(FILE *out, const solve_run *run)
{
    const ng_solve_stats *s = &run->outcome.stats;
    (void)fprintf(out,
                  "solver: %s\nnodes: %" PRIu64 "\nline solves: %" PRIu64
                  "\nsolutions found: %" PRIu32 "\nworkspace: %zu bytes\nelapsed: %" PRIu64 " ns\n",
                  run->solver_name, s->nodes, s->line_solves, s->solutions_found,
                  s->workspace_bytes, s->elapsed_ns);
}

static ng_status print_text(FILE *out, const solve_run *run)
{
    (void)fprintf(out, "%s\n", cli_result_text(run->outcome.result));
    if (run->args->values[OPT_COUNT] != nullptr) {
        print_solution_count(out, run);
    }
    ng_status status = NG_OK;
    if (run->outcome.result != NG_SOLVE_UNSOLVABLE) {
        const ng_writer w = ng_writer_from_file(out);
        const bool too_large = ng_puzzle_width(run->puzzle) > CLI_DEFAULT_MAX_RENDER ||
                               ng_puzzle_height(run->puzzle) > CLI_DEFAULT_MAX_RENDER;
        status = too_large ? ng_render_summary(run->puzzle, run->grid, &w)
                           : ng_render_text(run->puzzle, run->grid, nullptr, &w);
    }
    if (status == NG_OK && run->args->stats) {
        print_stats_text(out, run);
    }
    return status;
}

static char cell_char(ng_cell cell)
{
    switch (cell) {
    case NG_CELL_FILLED:
        return '#';
    case NG_CELL_EMPTY:
        return '.';
    case NG_CELL_UNKNOWN:
        break;
    }
    return '?';
}

static void print_json_grid(FILE *out, const solve_run *run)
{
    if (run->outcome.result == NG_SOLVE_UNSOLVABLE) {
        (void)fputs("null", out);
        return;
    }
    (void)fputc('[', out);
    for (ng_dim r = 0; r < ng_grid_height(run->grid); ++r) {
        (void)fputs(r == 0u ? "\"" : ", \"", out);
        for (ng_dim c = 0; c < ng_grid_width(run->grid); ++c) {
            (void)fputc(cell_char(ng_grid_get(run->grid, r, c)), out);
        }
        (void)fputc('"', out);
    }
    (void)fputc(']', out);
}

/* Schema nonogram-solve/1 (FILE_FORMATS.md §4). */
static void print_json(FILE *out, const solve_run *run)
{
    const ng_solve_stats *s = &run->outcome.stats;
    (void)fputs("{\n  \"schema\": \"nonogram-solve/1\",\n  \"puzzle\": ", out);
    cli_print_json_string(out, run->args->puzzle_path);
    (void)fputs(",\n  \"solver\": ", out);
    cli_print_json_string(out, run->solver_name);
    (void)fprintf(out, ",\n  \"result\": \"%s\",\n  \"verified\": %s,\n  \"count_exact\": %s,\n",
                  cli_result_json(run->outcome.result),
                  run->outcome.result == NG_SOLVE_SOLVED ? "true" : "false",
                  count_is_exact(run) ? "true" : "false");
    (void)fprintf(out,
                  "  \"stats\": { \"nodes\": %" PRIu64 ", \"line_solves\": %" PRIu64
                  ", \"elapsed_ns\": %" PRIu64 ", \"workspace_bytes\": %zu"
                  ", \"solutions_found\": %" PRIu32 " },\n  \"grid\": ",
                  s->nodes, s->line_solves, s->elapsed_ns, s->workspace_bytes, s->solutions_found);
    print_json_grid(out, run);
    (void)fputs("\n}\n", out);
}

static int write_stdout(FILE *out, FILE *err, const solve_run *run)
{
    ng_status status = NG_OK;
    if (run->args->json) {
        print_json(out, run);
    } else {
        status = print_text(out, run);
    }
    if (status != NG_OK || ferror(out) != 0 || fflush(out) != 0) {
        (void)fprintf(err, "nonogram: cannot write the output: %s\n",
                      status != NG_OK ? ng_status_str(status) : strerror(errno));
        return status == NG_OK || status == NG_ERR_IO ? CLI_EXIT_INPUT : CLI_EXIT_INTERNAL;
    }
    return CLI_EXIT_SUCCESS;
}

static FILE *open_output(FILE *err, const char *path)
{
    FILE *file = fopen(path, "wb");
    if (file == nullptr) {
        (void)fprintf(err, "nonogram: cannot open '%s': %s\n", path, strerror(errno));
    }
    return file;
}

/* --output: the solved grid as a .sol file. */
static int write_solution(FILE *err, const char *path, const ng_grid *grid)
{
    FILE *file = open_output(err, path);
    if (file == nullptr) {
        return CLI_EXIT_INPUT;
    }
    const ng_writer w = ng_writer_from_file(file);
    const ng_status status = ng_io_write_sol(grid, &w);
    const bool closed = fclose(file) == 0;
    if (status != NG_OK || !closed) {
        (void)fprintf(err, "nonogram: cannot write '%s': %s\n", path,
                      status != NG_OK ? ng_status_str(status) : strerror(errno));
        return CLI_EXIT_INPUT;
    }
    return CLI_EXIT_SUCCESS;
}

/* F-32: a SOLVED grid must be VALID; anything else is a solver bug. */
static int check_solution(FILE *err, const solve_run *run)
{
    ng_verify_report report = {};
    if (ng_verify(run->puzzle, run->grid, &report) != NG_OK || report.verdict != NG_VERDICT_VALID) {
        (void)fprintf(err, "nonogram: internal error: solver '%s' returned an invalid grid\n",
                      run->solver_name);
        return CLI_EXIT_INTERNAL;
    }
    return CLI_EXIT_SUCCESS;
}

/* Exit code of a solve that ran without error. */
static int report_outcome(FILE *out, FILE *err, const solve_run *run)
{
    const bool solved = run->outcome.result == NG_SOLVE_SOLVED;
    int code = solved ? check_solution(err, run) : CLI_EXIT_SUCCESS;
    if (code == CLI_EXIT_SUCCESS) {
        code = write_stdout(out, err, run);
    }
    if (code == CLI_EXIT_SUCCESS && solved && run->args->values[OPT_OUTPUT] != nullptr) {
        code = write_solution(err, run->args->values[OPT_OUTPUT], run->grid);
    }
    if (code == CLI_EXIT_SUCCESS && !solved) {
        code = CLI_EXIT_FAILURE;
    }
    return code;
}

/* Exit code of a solve that failed with @p status. */
static int report_failure(FILE *err, const char *solver_name, ng_status status)
{
    if (status == NG_ERR_LIMIT) {
        (void)fprintf(err,
                      "nonogram: solver '%s' cannot take this puzzle within its limits (puzzle "
                      "size or --max-memory)\n",
                      solver_name);
        return CLI_EXIT_USAGE;
    }
    (void)fprintf(err, "nonogram: solver '%s' failed: %s\n", solver_name, ng_status_str(status));
    return CLI_EXIT_INTERNAL;
}

static int run_solver(FILE *out, FILE *err, const solve_args *args, const ng_solver_vtable *solver,
                      const ng_puzzle *puzzle)
{
    ng_grid *grid = nullptr;
    ng_status status = ng_grid_create(ng_allocator_default(), ng_puzzle_width(puzzle),
                                      ng_puzzle_height(puzzle), &grid);
    if (status != NG_OK) {
        (void)fprintf(err, "nonogram: cannot create the grid: %s\n", ng_status_str(status));
        return CLI_EXIT_INTERNAL;
    }
    solve_run run = {.args = args, .solver_name = solver->name, .puzzle = puzzle, .grid = grid};
    status = ng_solve(ng_allocator_default(), solver, puzzle, &args->options, grid, &run.outcome);
    const int code = status == NG_OK ? report_outcome(out, err, &run)
                                     : report_failure(err, solver->name, status);
    ng_grid_destroy(grid);
    return code;
}

int cli_solve(int argc, char **argv, FILE *out, FILE *err)
{
    return cli_solve_with(argc, argv, out, err, nullptr);
}

int cli_solve_with(int argc, char **argv, FILE *out, FILE *err, const ng_solver_vtable *solver)
{
    solve_args args;
    int code = parse_args(argc, argv, &args, err);
    if (code != CLI_EXIT_SUCCESS) {
        return code;
    }
    if (solver == nullptr) {
        solver = cli_choose_solver(args.values[OPT_SOLVER]);
    }
    if (solver == nullptr) {
        return cli_usage_error(err, "unknown solver", args.values[OPT_SOLVER]);
    }
    ng_puzzle *puzzle = nullptr;
    code = cli_load_puzzle(err, args.puzzle_path, &puzzle);
    if (code == CLI_EXIT_SUCCESS) {
        code = run_solver(out, err, &args, solver, puzzle);
    }
    ng_puzzle_destroy(puzzle);
    return code;
}
