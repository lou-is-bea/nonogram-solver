#include <errno.h>
#include <inttypes.h>
#include <stdalign.h>
#include <stdckdint.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "nonogram/allocator.h"
#include "nonogram/puzzle.h"
#include "nonogram/solver.h"
#include "nonogram/status.h"

#include "bench.h"
#include "cli.h"

/* nonogram bench <puzzle.non>... [--solver <name>] [--runs <n>] [--warmup <n>] [--seed <n>]
 *                [--format text|json]
 * Measures the solver on each puzzle with the harness (TESTING.md §4.8). The result of a
 * puzzle does not change the exit code: 0 measured; 2 usage, including a puzzle the solver
 * cannot take (NG_ERR_LIMIT); 3 input or output; 4 internal, including a solver that breaks
 * its contract (invalid SOLVED grid, runs that differ). */

enum bench_option : uint8_t {
    OPT_SOLVER,
    OPT_RUNS,
    OPT_WARMUP,
    OPT_SEED,
    OPT_FORMAT,
    OPT_VALUE_COUNT,
};

static const char *const option_names[OPT_VALUE_COUNT] = {
    "--solver", "--runs", "--warmup", "--seed", "--format",
};

typedef struct bench_args {
    const char *values[OPT_VALUE_COUNT]; /* nullptr: option absent */
    size_t puzzle_count;
    bool json;
    ngi_bench_options options;
} bench_args;

/* Handles one argument; returns CLI_EXIT_SUCCESS or a usage error. */
static int parse_arg(int argc, char **argv, int *index, bench_args *args, FILE *err)
{
    const char *arg = argv[*index];
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
    ++args->puzzle_count;
    return CLI_EXIT_SUCCESS;
}

/* A count in [0, max]: zeros only ("0", "00"), or what cli_parse_count accepts, so that
 * leading zeros are treated alike for every value. */
static bool parse_number(const char *text, uint64_t max, uint64_t *out)
{
    if (*text != '\0' && strspn(text, "0") == strlen(text)) {
        *out = 0;
        return true;
    }
    return cli_parse_count(text, max, out);
}

/* Converts the numeric options and --format; returns CLI_EXIT_SUCCESS or a usage error. */
static int convert_values(bench_args *args, FILE *err)
{
    uint64_t number = 0;
    const char *runs = args->values[OPT_RUNS];
    if (runs != nullptr) {
        if (!cli_parse_count(runs, NGI_BENCH_MAX_RUNS, &number)) {
            return cli_usage_error(err, "invalid --runs value", runs);
        }
        args->options.measured_runs = (uint32_t)number;
    }
    const char *warmup = args->values[OPT_WARMUP];
    if (warmup != nullptr) {
        if (!parse_number(warmup, NGI_BENCH_MAX_RUNS, &number)) {
            return cli_usage_error(err, "invalid --warmup value", warmup);
        }
        args->options.warmup_runs = (uint32_t)number;
    }
    const char *seed = args->values[OPT_SEED];
    if (seed != nullptr && !parse_number(seed, UINT64_MAX, &args->options.solve.seed)) {
        return cli_usage_error(err, "invalid --seed value", seed);
    }
    const char *format = args->values[OPT_FORMAT];
    if (format != nullptr && strcmp(format, "text") != 0 && strcmp(format, "json") != 0) {
        return cli_usage_error(err, "invalid --format value", format);
    }
    args->json = format != nullptr && strcmp(format, "json") == 0;
    return CLI_EXIT_SUCCESS;
}

static int parse_args(int argc, char **argv, bench_args *args, FILE *err)
{
    *args = (bench_args){.options = ngi_bench_options_default()};
    /* argv[argc] is the only null entry; the check keeps the static analyser informed. */
    for (int i = 0; i < argc && argv[i] != nullptr; ++i) {
        const int code = parse_arg(argc, argv, &i, args, err);
        if (code != CLI_EXIT_SUCCESS) {
            return code;
        }
    }
    if (args->puzzle_count == 0u) {
        return cli_usage_error(err, "bench: missing puzzle file", nullptr);
    }
    return convert_values(args, err);
}

/* Whether @p arg is an option whose value is the next argument ("--runs 5"). The arguments
 * were parsed already, so the value is there. */
static bool value_follows(const char *arg)
{
    for (size_t i = 0; i < OPT_VALUE_COUNT; ++i) {
        if (strcmp(arg, option_names[i]) == 0) {
            return true;
        }
    }
    return false;
}

/* Index of the puzzle argument that follows argv[*index - 1], or argc when none is left. */
static int next_puzzle(int argc, char **argv, int index)
{
    for (int i = index; i < argc && argv[i] != nullptr; ++i) {
        if (value_follows(argv[i])) {
            ++i;
        } else if (argv[i][0] != '-') {
            return i;
        }
    }
    return argc;
}

/* What the measurements produced, for the outputs. */
typedef struct bench_report {
    const bench_args *args;
    const char *solver_name;
    int argc;
    char **argv;
    const ngi_bench_result *results; /* one per puzzle, in the order of the arguments */
} bench_report;

static void print_text(FILE *out, const bench_report *report)
{
    const ngi_bench_options *o = &report->args->options;
    (void)fprintf(out,
                  "solver: %s, measured runs: %" PRIu32 ", warmup runs: %" PRIu32 ", seed: %" PRIu64
                  "\n",
                  report->solver_name, o->measured_runs, o->warmup_runs, o->solve.seed);
    size_t k = 0;
    for (int i = next_puzzle(report->argc, report->argv, 0); i < report->argc;
         i = next_puzzle(report->argc, report->argv, i + 1), ++k) {
        const ngi_bench_result *r = &report->results[k];
        (void)fprintf(out,
                      "%s: %s, nodes %" PRIu64 ", median %" PRIu64 " ns (min %" PRIu64
                      ", max %" PRIu64 "), peak %zu bytes\n",
                      report->argv[i], cli_result_text(r->result), r->stats.nodes, r->median_ns,
                      r->min_ns, r->max_ns, r->peak_bytes);
    }
}

static void print_json_result(FILE *out, const char *path, const ngi_bench_result *r)
{
    (void)fputs("    { \"puzzle\": ", out);
    cli_print_json_string(out, path);
    (void)fprintf(out,
                  ", \"result\": \"%s\", \"nodes\": %" PRIu64 ", \"line_solves\": %" PRIu64
                  ", \"solutions_found\": %" PRIu32 ", \"workspace_bytes\": %zu"
                  ", \"peak_bytes\": %zu, \"median_ns\": %" PRIu64 ", \"min_ns\": %" PRIu64
                  ", \"max_ns\": %" PRIu64 " }",
                  cli_result_json(r->result), r->stats.nodes, r->stats.line_solves,
                  r->stats.solutions_found, r->stats.workspace_bytes, r->peak_bytes, r->median_ns,
                  r->min_ns, r->max_ns);
}

/* Schema nonogram-bench/1 (FILE_FORMATS.md §4). */
static void print_json(FILE *out, const bench_report *report)
{
    const ngi_bench_options *o = &report->args->options;
    (void)fputs("{\n  \"schema\": \"nonogram-bench/1\",\n  \"solver\": ", out);
    cli_print_json_string(out, report->solver_name);
    (void)fprintf(out,
                  ",\n  \"clock\": \"monotonic\",\n  \"seed\": %" PRIu64
                  ",\n  \"warmup_runs\": %" PRIu32 ",\n  \"measured_runs\": %" PRIu32
                  ",\n  \"results\": [\n",
                  o->solve.seed, o->warmup_runs, o->measured_runs);
    size_t k = 0;
    for (int i = next_puzzle(report->argc, report->argv, 0); i < report->argc;
         i = next_puzzle(report->argc, report->argv, i + 1), ++k) {
        (void)fputs(k == 0u ? "" : ",\n", out);
        print_json_result(out, report->argv[i], &report->results[k]);
    }
    (void)fputs("\n  ]\n}\n", out);
}

static int write_stdout(FILE *out, FILE *err, const bench_report *report)
{
    if (report->args->json) {
        print_json(out, report);
    } else {
        print_text(out, report);
    }
    if (ferror(out) != 0 || fflush(out) != 0) {
        (void)fprintf(err, "nonogram: cannot write the output: %s\n", strerror(errno));
        return CLI_EXIT_INPUT;
    }
    return CLI_EXIT_SUCCESS;
}

/* Exit code of a measurement of @p path that failed with @p status. */
static int report_failure(FILE *err, const char *solver_name, const char *path, ng_status status,
                          ngi_bench_defect defect)
{
    if (status == NG_ERR_LIMIT) {
        (void)fprintf(err, "nonogram: solver '%s' cannot take '%s' within its limits\n",
                      solver_name, path);
        return CLI_EXIT_USAGE;
    }
    if (status == NG_ERR_INTERNAL && defect == NGI_BENCH_INVALID_GRID) {
        (void)fprintf(err,
                      "nonogram: internal error: solver '%s' returned an invalid grid for '%s'\n",
                      solver_name, path);
    } else if (status == NG_ERR_INTERNAL && defect == NGI_BENCH_NOT_DETERMINISTIC) {
        (void)fprintf(err, "nonogram: internal error: solver '%s' is not deterministic on '%s'\n",
                      solver_name, path);
    } else {
        (void)fprintf(err, "nonogram: solver '%s' failed on '%s': %s\n", solver_name, path,
                      ng_status_str(status));
    }
    return CLI_EXIT_INTERNAL;
}

/* Loads and measures the puzzle at @p path into @p result. */
static int measure_puzzle(FILE *err, const bench_args *args, const ng_solver_vtable *solver,
                          const char *path, ngi_bench_result *result)
{
    ng_puzzle *puzzle = nullptr;
    int code = cli_load_puzzle(err, path, &puzzle);
    if (code == CLI_EXIT_SUCCESS) {
        const ng_status status =
            ngi_bench_measure(ng_allocator_default(), solver, puzzle, &args->options, result);
        if (status != NG_OK) {
            code = report_failure(err, solver->name, path, status, result->defect);
        }
    }
    ng_puzzle_destroy(puzzle);
    return code;
}

static int measure_all(FILE *out, FILE *err, bench_report *report, const ng_solver_vtable *solver)
{
    const ng_allocator *allocator = ng_allocator_default();
    size_t bytes = 0;
    if (ckd_mul(&bytes, report->args->puzzle_count, sizeof(ngi_bench_result))) {
        (void)fprintf(err, "nonogram: too many puzzles\n");
        return CLI_EXIT_USAGE;
    }
    ngi_bench_result *results = allocator->alloc(allocator->ctx, bytes, alignof(ngi_bench_result));
    if (results == nullptr) {
        (void)fprintf(err, "nonogram: %s\n", ng_status_str(NG_ERR_OUT_OF_MEMORY));
        return CLI_EXIT_INTERNAL;
    }
    int code = CLI_EXIT_SUCCESS;
    size_t k = 0;
    for (int i = next_puzzle(report->argc, report->argv, 0);
         i < report->argc && code == CLI_EXIT_SUCCESS;
         i = next_puzzle(report->argc, report->argv, i + 1), ++k) {
        results[k] = (ngi_bench_result){};
        code = measure_puzzle(err, report->args, solver, report->argv[i], &results[k]);
    }
    if (code == CLI_EXIT_SUCCESS) {
        report->results = results;
        code = write_stdout(out, err, report);
    }
    allocator->free(allocator->ctx, results, bytes);
    return code;
}

int cli_bench(int argc, char **argv, FILE *out, FILE *err)
{
    return cli_bench_with(argc, argv, out, err, nullptr);
}

int cli_bench_with(int argc, char **argv, FILE *out, FILE *err, const ng_solver_vtable *solver)
{
    bench_args args;
    const int code = parse_args(argc, argv, &args, err);
    if (code != CLI_EXIT_SUCCESS) {
        return code;
    }
    if (solver == nullptr) {
        solver = cli_choose_solver(args.values[OPT_SOLVER]);
    }
    if (solver == nullptr) {
        return cli_usage_error(err, "unknown solver", args.values[OPT_SOLVER]);
    }
    bench_report report = {.args = &args, .solver_name = solver->name, .argc = argc, .argv = argv};
    return measure_all(out, err, &report, solver);
}
