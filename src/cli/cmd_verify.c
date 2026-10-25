#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "nonogram/allocator.h"
#include "nonogram/grid.h"
#include "nonogram/puzzle.h"
#include "nonogram/status.h"
#include "nonogram/types.h"
#include "nonogram/verify.h"

#include "cli.h"

/* nonogram verify <puzzle.non> <grid.sol>: prints the verdict (F-20, F-21) on stdout. */

static void print_runs(FILE *out, const ng_dim *runs, ng_run_count count)
{
    if (count == 0u) {
        (void)fputs("0", out);
    }
    for (ng_run_count i = 0; i < count; ++i) {
        (void)fprintf(out, i == 0u ? "%" NG_PRI_DIM : " %" NG_PRI_DIM, runs[i]);
    }
}

/* "invalid: row 2 has runs 1 2, expected 1 1" (rows and columns numbered from 1). */
static int print_invalid(FILE *out, FILE *err, const ng_puzzle *puzzle, const ng_grid *grid,
                         ng_line_id line)
{
    const ng_allocator *a = ng_allocator_default();
    const ng_run_count expected_count = ng_puzzle_line_run_count(puzzle, line);
    ng_run_count observed_count = 0;
    if (ng_verify_line_runs(grid, line, nullptr, 0, &observed_count) != NG_OK) {
        return CLI_EXIT_INTERNAL;
    }
    /* A line of n cells holds at most (n + 1) / 2 runs: these sizes cannot overflow. */
    const size_t capacity =
        (size_t)(expected_count > observed_count ? expected_count : observed_count) + 1u;
    ng_dim *expected = a->alloc(a->ctx, capacity * sizeof(ng_dim), alignof(ng_dim));
    ng_dim *observed = a->alloc(a->ctx, capacity * sizeof(ng_dim), alignof(ng_dim));
    int code = CLI_EXIT_FAILURE;
    if (expected == nullptr || observed == nullptr ||
        ng_puzzle_copy_line_runs(puzzle, line, expected, expected_count) != NG_OK ||
        ng_verify_line_runs(grid, line, observed, observed_count, &observed_count) != NG_OK) {
        (void)fputs("nonogram: out of memory\n", err);
        code = CLI_EXIT_INTERNAL;
    } else {
        const ng_dim height = ng_puzzle_height(puzzle);
        const bool is_row = line < height;
        (void)fprintf(out, "invalid: %s %" NG_PRI_LINE_ID " has runs ", is_row ? "row" : "column",
                      (is_row ? line : line - height) + 1u);
        print_runs(out, observed, observed_count);
        (void)fputs(", expected ", out);
        print_runs(out, expected, expected_count);
        (void)fputc('\n', out);
    }
    a->free(a->ctx, expected, capacity * sizeof(ng_dim));
    a->free(a->ctx, observed, capacity * sizeof(ng_dim));
    return code;
}

static int print_verdict(FILE *out, FILE *err, const ng_puzzle *puzzle, const ng_grid *grid)
{
    ng_verify_report report = {};
    if (ng_verify(puzzle, grid, &report) != NG_OK) {
        (void)fputs("nonogram: internal error: verification refused its arguments\n", err);
        return CLI_EXIT_INTERNAL;
    }
    switch (report.verdict) {
    case NG_VERDICT_VALID:
        (void)fputs("valid\n", out);
        return CLI_EXIT_SUCCESS;
    case NG_VERDICT_INCOMPLETE:
        (void)fprintf(out, "incomplete: %" NG_PRI_CELL_INDEX " unknown cells\n",
                      report.unknown_cells);
        return CLI_EXIT_FAILURE;
    case NG_VERDICT_INVALID:
        return print_invalid(out, err, puzzle, grid, report.first_bad_line);
    }
    return CLI_EXIT_INTERNAL;
}

int cli_verify(int argc, char **argv, FILE *out, FILE *err)
{
    if (argc != 2 || argv[0] == nullptr || argv[1] == nullptr) {
        cli_report_usage_error(err, "verify: expected <puzzle.non> <grid.sol>", nullptr);
        return CLI_EXIT_USAGE;
    }
    for (int i = 0; i < argc; ++i) {
        if (argv[i][0] == '-') {
            cli_report_usage_error(err, "unknown option", argv[i]);
            return CLI_EXIT_USAGE;
        }
    }
    ng_puzzle *puzzle = nullptr;
    ng_grid *grid = nullptr;
    int code = cli_load_puzzle(err, argv[0], &puzzle);
    if (code == CLI_EXIT_SUCCESS) {
        code = cli_load_grid(err, argv[1], puzzle, &grid);
    }
    if (code == CLI_EXIT_SUCCESS) {
        code = print_verdict(out, err, puzzle, grid);
        if (fflush(out) != 0) {
            (void)fputs("nonogram: cannot write the output\n", err);
            code = CLI_EXIT_INPUT;
        }
    }
    ng_grid_destroy(grid);
    ng_puzzle_destroy(puzzle);
    return code;
}
