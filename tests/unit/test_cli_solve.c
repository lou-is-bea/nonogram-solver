#include <criterion/criterion.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "nonogram/grid.h"
#include "nonogram/puzzle.h"
#include "nonogram/solver.h"
#include "nonogram/status.h"

#include "cli.h"
#include "ng_test_fixture.h"

/* nonogram solve driven with fake solvers: what follows the solve, which the solvers of the
 * registry cannot reach (a wrong SOLVED grid, an INCOMPLETE result with a count). */
TestSuite(cli_solve, .init = ng_test_setup, .fini = ng_test_teardown, .timeout = 10);

static constexpr size_t capture_size = 4'096;

/* l_shape is 3 x 4 and has a unique solution: a grid fully FILLED is not it. */
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

/* Claims SOLVED with a grid that breaks the clues: the CLI must catch it (F-32). */
static ng_status wrong_solve(void *workspace, ng_grid *inout, ng_solve_stats *stats,
                             ng_solve_result *result)
{
    (void)workspace;
    ng_grid_fill(inout, NG_CELL_FILLED);
    stats->solutions_found = 1;
    *result = NG_SOLVE_SOLVED;
    return NG_OK;
}

/* Stuck after one solution: the count is only a lower bound. */
static ng_status stuck_solve(void *workspace, ng_grid *inout, ng_solve_stats *stats,
                             ng_solve_result *result)
{
    (void)workspace;
    (void)inout;
    stats->solutions_found = 1;
    *result = NG_SOLVE_INCOMPLETE;
    return NG_OK;
}

static const ng_solver_vtable wrong_solver = {
    .name = "wrong",
    .summary = "returns an invalid grid",
    .complete = true,
    .workspace_size = no_workspace,
    .init = nothing_to_prepare,
    .solve = wrong_solve,
};

static const ng_solver_vtable stuck_solver = {
    .name = "stuck",
    .summary = "incomplete after one solution",
    .complete = false,
    .workspace_size = no_workspace,
    .init = nothing_to_prepare,
    .solve = stuck_solve,
};

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

/* Runs "nonogram solve <puzzle_path> <option...>" with @p solver. */
static captured run(const ng_solver_vtable *solver, char **options, int option_count)
{
    char *argv[8] = {puzzle_path};
    cr_assert_lt(option_count, 7);
    for (int i = 0; i < option_count; ++i) {
        argv[i + 1] = options[i];
    }
    FILE *out = tmpfile();
    FILE *err = tmpfile();
    cr_assert_not_null(out);
    cr_assert_not_null(err);
    captured result = {.code = cli_solve_with(option_count + 1, argv, out, err, solver)};
    read_back(out, result.out);
    read_back(err, result.err);
    return result;
}

Test(cli_solve, invalid_solved_grid_is_an_internal_error_and_nothing_is_printed)
{
    const captured r = run(&wrong_solver, nullptr, 0);

    cr_assert_eq(r.code, CLI_EXIT_INTERNAL);
    cr_assert_str_empty(r.out);
    cr_assert_not_null(strstr(r.err, "solver 'wrong' returned an invalid grid"), "%s", r.err);
}

Test(cli_solve, invalid_solved_grid_is_not_written_to_the_output_file)
{
    char path[] = NG_BINARY_DIR "/cli_solve_wrong.sol";
    char output_option[] = "--output";
    char *options[] = {output_option, path};
    (void)remove(path);

    const captured r = run(&wrong_solver, options, 2);

    cr_assert_eq(r.code, CLI_EXIT_INTERNAL);
    FILE *file = fopen(path, "rb");
    cr_assert_null(file, "no file must be written");
}

Test(cli_solve, incomplete_result_gives_a_lower_bound_count)
{
    char count_option[] = "--count=3";
    char *options[] = {count_option};

    const captured r = run(&stuck_solver, options, 1);

    cr_assert_eq(r.code, CLI_EXIT_FAILURE);
    cr_assert_not_null(strstr(r.out, "incomplete\nsolutions: at least 1\n"), "%s", r.out);
}

Test(cli_solve, incomplete_result_has_an_inexact_count_in_json)
{
    char count_option[] = "--count=3";
    char format_option[] = "--format=json";
    char *options[] = {count_option, format_option};

    const captured r = run(&stuck_solver, options, 2);

    cr_assert_eq(r.code, CLI_EXIT_FAILURE);
    cr_assert_not_null(strstr(r.out, "\"result\": \"INCOMPLETE\""), "%s", r.out);
    cr_assert_not_null(strstr(r.out, "\"count_exact\": false"), "%s", r.out);
}

/* cli_parse_count, shared by solve and show: [1, max], digits only, *out untouched on failure. */
TestSuite(cli_parse_count, .init = ng_test_setup, .fini = ng_test_teardown, .timeout = 10);

static constexpr uint64_t untouched = 77;

Test(cli_parse_count, values_within_the_range_are_accepted)
{
    uint64_t value = untouched;

    cr_assert(cli_parse_count("1", UINT64_MAX, &value));
    cr_assert_eq(value, 1u);
    cr_assert(cli_parse_count("250", 250, &value));
    cr_assert_eq(value, 250u);
    cr_assert(cli_parse_count("0042", 100, &value));
    cr_assert_eq(value, 42u);
    cr_assert(cli_parse_count("18446744073709551615", UINT64_MAX, &value));
    cr_assert_eq(value, UINT64_MAX);
}

Test(cli_parse_count, invalid_text_is_rejected_and_out_is_unchanged)
{
    static const char *const rejected[] = {"", "0", "000", "-1", "+1", " 1", "1 ", "1x", "1.5"};
    for (size_t i = 0; i < sizeof rejected / sizeof rejected[0]; ++i) {
        uint64_t value = untouched;

        cr_assert_not(cli_parse_count(rejected[i], UINT64_MAX, &value), "'%s'", rejected[i]);
        cr_assert_eq(value, untouched, "'%s'", rejected[i]);
    }
}

Test(cli_parse_count, values_beyond_the_maximum_are_rejected_and_out_is_unchanged)
{
    uint64_t value = untouched;

    cr_assert_not(cli_parse_count("251", 250, &value));
    cr_assert_not(cli_parse_count("9", 8, &value));
    cr_assert_not(cli_parse_count("18446744073709551616", UINT64_MAX, &value));
    cr_assert_not(cli_parse_count("99999999999999999999999", UINT64_MAX, &value));
    cr_assert_eq(value, untouched);
}
