#include <criterion/criterion.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "nonogram/grid.h"
#include "nonogram/puzzle.h"
#include "nonogram/solver.h"
#include "nonogram/status.h"
#include "nonogram/types.h"

#include "cli.h"
#include "ng_test_fixture.h"

/* nonogram bench driven with fake solvers: the reports of a solver that breaks its contract,
 * which the solvers of the registry cannot reach. */
TestSuite(cli_bench, .init = ng_test_setup, .fini = ng_test_teardown, .timeout = 10);

static constexpr size_t capture_size = 4'096;

static char puzzle_path[] = NG_SOURCE_DIR "/tests/data/puzzles/tiny/l_shape.non";

static ng_status no_workspace(const ng_puzzle *puzzle, const ng_solve_options *options, size_t *out)
{
    (void)puzzle;
    (void)options;
    *out = 0;
    return NG_OK;
}

static ng_status nothing_to_prepare(void *workspace, size_t workspace_size, const ng_puzzle *puzzle,
                                    const ng_solve_options *options)
{
    (void)workspace;
    (void)workspace_size;
    (void)puzzle;
    (void)options;
    return NG_OK;
}

/* Claims SOLVED with a grid that breaks the clues. */
static ng_status wrong_solve(void *workspace, ng_grid *inout, ng_solve_stats *stats,
                             ng_solve_result *result)
{
    (void)workspace;
    ng_grid_fill(inout, NG_CELL_FILLED);
    stats->solutions_found = 1;
    *result = NG_SOLVE_SOLVED;
    return NG_OK;
}

/* Visits one more node at each call; a test process runs one test. */
static ng_counter drifting_nodes = 0;

static ng_status drifting_solve(void *workspace, ng_grid *inout, ng_solve_stats *stats,
                                ng_solve_result *result)
{
    (void)workspace;
    (void)inout;
    stats->nodes = ++drifting_nodes;
    *result = NG_SOLVE_INCOMPLETE;
    return NG_OK;
}

/* Fails with an error other than a limit. */
static ng_status failing_solve(void *workspace, ng_grid *inout, ng_solve_stats *stats,
                               ng_solve_result *result)
{
    (void)workspace;
    (void)inout;
    (void)stats;
    *result = NG_SOLVE_SOLVED; /* meaningless on error (solver.h): must be ignored */
    return NG_ERR_IO;
}

#define FAKE_SOLVER(solver_name, solve_function)                                                   \
    {.name = (solver_name),                                                                        \
     .summary = "fake",                                                                            \
     .complete = false,                                                                            \
     .workspace_size = no_workspace,                                                               \
     .init = nothing_to_prepare,                                                                   \
     .solve = (solve_function)}

static const ng_solver_vtable wrong_solver = FAKE_SOLVER("wrong", wrong_solve);
static const ng_solver_vtable drifting_solver = FAKE_SOLVER("drifting", drifting_solve);
static const ng_solver_vtable failing_solver = FAKE_SOLVER("failing", failing_solve);

typedef struct captured {
    int code;
    char out[capture_size];
    char err[capture_size];
} captured;

static void read_back(FILE *stream, char *buffer)
{
    cr_assert_eq(fflush(stream), 0);
    cr_assert_eq(fseek(stream, 0, SEEK_SET), 0);
    const size_t length = fread(buffer, 1, capture_size - 1u, stream);
    cr_assert_eq(ferror(stream), 0);
    buffer[length] = '\0';
    cr_assert_eq(fclose(stream), 0);
}

/* Runs "nonogram bench <puzzle_path>" with @p solver. */
static captured run(const ng_solver_vtable *solver)
{
    char *argv[] = {puzzle_path, nullptr};
    FILE *out = tmpfile();
    FILE *err = tmpfile();
    cr_assert_not_null(out);
    cr_assert_not_null(err);
    captured result = {.code = cli_bench_with(1, argv, out, err, solver)};
    read_back(out, result.out);
    read_back(err, result.err);
    return result;
}

Test(cli_bench, invalid_solved_grid_is_an_internal_error_and_nothing_is_printed)
{
    const captured r = run(&wrong_solver);

    cr_assert_eq(r.code, CLI_EXIT_INTERNAL);
    cr_assert_str_empty(r.out);
    cr_assert_not_null(strstr(r.err, "solver 'wrong' returned an invalid grid for '"), "%s", r.err);
}

Test(cli_bench, runs_that_differ_are_an_internal_error_and_nothing_is_printed)
{
    const captured r = run(&drifting_solver);

    cr_assert_eq(r.code, CLI_EXIT_INTERNAL);
    cr_assert_str_empty(r.out);
    cr_assert_not_null(strstr(r.err, "solver 'drifting' is not deterministic on '"), "%s", r.err);
}

Test(cli_bench, other_solver_errors_are_internal_errors)
{
    const captured r = run(&failing_solver);

    cr_assert_eq(r.code, CLI_EXIT_INTERNAL);
    cr_assert_str_empty(r.out);
    cr_assert_not_null(strstr(r.err, "solver 'failing' failed on '"), "%s", r.err);
    cr_assert_not_null(strstr(r.err, "': input/output error\n"), "%s", r.err);
}
