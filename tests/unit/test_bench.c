#include <criterion/criterion.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#include "nonogram/grid.h"
#include "nonogram/io.h"
#include "nonogram/puzzle.h"
#include "nonogram/solver.h"
#include "nonogram/status.h"
#include "nonogram/stream.h"
#include "nonogram/types.h"

#include "bench.h"
#include "ng_test_fixture.h"
#include "test_puzzles.h"

TestSuite(bench_measure, .init = ng_test_setup, .fini = ng_test_teardown, .timeout = 10);
TestSuite(bench_clock, .init = ng_test_setup, .fini = ng_test_teardown, .timeout = 10);

/* An L in a 3 x 4 grid: unique solution, 2^12 candidates for S0. */
static const char *const l_rows[] = {"#..", "#..", "#..", "###"};
static constexpr ng_dim l_height = 4;

/* 30 cells: beyond the 25 of S0. */
static const char *const wide_rows[] = {"#.#.#.", ".#.#.#", "#.#.#.", ".#.#.#", "#.#.#."};
static constexpr ng_dim wide_height = 5;

static ng_puzzle *puzzle_of(const char *const *rows, ng_dim height)
{
    ng_grid *grid = ng_test_grid_of(rows, height);
    ng_puzzle *puzzle = ng_test_puzzle_from_grid(grid);
    ng_grid_destroy(grid);
    return puzzle;
}

static ng_puzzle *read_puzzle(const char *path)
{
    FILE *file = fopen(path, "rb");
    cr_assert_not_null(file, "cannot open %s", path);
    const ng_reader r = ng_reader_from_file(file);
    ng_puzzle *puzzle = nullptr;
    cr_assert_eq(ng_io_read_non(&ng_test_heap()->base, &r, &puzzle, nullptr, nullptr), NG_OK);
    cr_assert_eq(fclose(file), 0);
    return puzzle;
}

static ngi_bench_options quick_options(void)
{
    ngi_bench_options options = ngi_bench_options_default();
    options.warmup_runs = 1;
    options.measured_runs = 3;
    return options;
}

/* --- Fake solvers: what the solvers of the registry cannot do -------------------------- */

static constexpr size_t fake_workspace = 1'000;

static ng_status fixed_workspace(const ng_puzzle *puzzle, const ng_solve_options *options,
                                 size_t *out)
{
    (void)puzzle;
    (void)options;
    *out = fake_workspace;
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

/* Reads no clock and gives up at once: two clock readings per run, both from ng_solve. */
static ng_status give_up(void *workspace, ng_grid *inout, ng_solve_stats *stats,
                         ng_solve_result *result)
{
    (void)workspace;
    (void)inout;
    stats->nodes = 1;
    *result = NG_SOLVE_INCOMPLETE;
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

/* Visits one more node at each call. A test process runs one test, so the counter starts at 0
 * for the test that uses it. */
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

#define FAKE_SOLVER(solve_function)                                                                \
    {.name = #solve_function,                                                                      \
     .summary = "fake",                                                                            \
     .complete = false,                                                                            \
     .workspace_size = fixed_workspace,                                                            \
     .init = nothing_to_prepare,                                                                   \
     .solve = (solve_function)}

static const ng_solver_vtable giving_up_solver = FAKE_SOLVER(give_up);
static const ng_solver_vtable wrong_solver = FAKE_SOLVER(wrong_solve);
static const ng_solver_vtable drifting_solver = FAKE_SOLVER(drifting_solve);

/* --- Fake clock: run k lasts durations[k]; 1000 ns pass between two runs ----------------- */

typedef struct scripted_clock {
    const uint64_t *durations;
    size_t reading_count;
    uint64_t now;
} scripted_clock;

static constexpr uint64_t gap_between_runs = 1'000;

static uint64_t scripted_now_ns(void *ctx)
{
    scripted_clock *clock = ctx;
    const size_t reading = clock->reading_count++;
    clock->now += reading % 2u == 0u ? gap_between_runs : clock->durations[reading / 2u];
    return clock->now;
}

/* Measures giving_up_solver with the times of @p durations (warmup runs first). */
static ngi_bench_result measure_with_times(const uint64_t *durations, uint32_t warmup_runs,
                                           uint32_t measured_runs)
{
    scripted_clock state = {.durations = durations};
    const ng_clock clock = {.now_ns = scripted_now_ns, .ctx = &state};
    ngi_bench_options options = ngi_bench_options_default();
    options.warmup_runs = warmup_runs;
    options.measured_runs = measured_runs;
    options.solve.clock = &clock;
    ng_puzzle *puzzle = puzzle_of(l_rows, l_height);
    ngi_bench_result result = {};

    cr_assert_eq(
        ngi_bench_measure(&ng_test_heap()->base, &giving_up_solver, puzzle, &options, &result),
        NG_OK);

    cr_assert_eq(state.reading_count, (size_t)2 * (warmup_runs + measured_runs));
    ng_puzzle_destroy(puzzle);
    return result;
}

/* --- ngi_bench_options_default --------------------------------------------------------- */

Test(bench_measure, default_options_follow_testing_4_8)
{
    const ngi_bench_options options = ngi_bench_options_default();

    cr_assert_eq(options.warmup_runs, 2u);
    cr_assert_eq(options.measured_runs, 11u);
    cr_assert_eq(options.solve.max_solutions, 1u);
    cr_assert_eq(options.solve.clock, ngi_bench_clock());
}

/* --- Arguments ------------------------------------------------------------------------- */

Test(bench_measure, null_arguments_are_rejected_and_out_is_untouched)
{
    ng_puzzle *puzzle = puzzle_of(l_rows, l_height);
    const ng_allocator *a = &ng_test_heap()->base;
    const ng_solver_vtable *brute = ng_solver_find("brute");
    ngi_bench_result result = {.median_ns = 7};

    cr_assert_eq(ngi_bench_measure(nullptr, brute, puzzle, nullptr, &result),
                 NG_ERR_INVALID_ARGUMENT);
    cr_assert_eq(ngi_bench_measure(a, nullptr, puzzle, nullptr, &result), NG_ERR_INVALID_ARGUMENT);
    cr_assert_eq(ngi_bench_measure(a, brute, nullptr, nullptr, &result), NG_ERR_INVALID_ARGUMENT);
    cr_assert_eq(ngi_bench_measure(a, brute, puzzle, nullptr, nullptr), NG_ERR_INVALID_ARGUMENT);
    cr_assert_eq(result.median_ns, 7u);
    ng_puzzle_destroy(puzzle);
}

Test(bench_measure, numbers_of_runs_out_of_range_are_rejected)
{
    ng_puzzle *puzzle = puzzle_of(l_rows, l_height);
    const ng_solver_vtable *brute = ng_solver_find("brute");
    ngi_bench_result result = {};
    ngi_bench_options no_run = quick_options();
    no_run.measured_runs = 0;
    ngi_bench_options too_many_runs = quick_options();
    too_many_runs.measured_runs = NGI_BENCH_MAX_RUNS + 1u;
    ngi_bench_options too_many_warmups = quick_options();
    too_many_warmups.warmup_runs = NGI_BENCH_MAX_RUNS + 1u;
    ng_test_heap()->request_count = 0;

    cr_assert_eq(ngi_bench_measure(&ng_test_heap()->base, brute, puzzle, &no_run, &result),
                 NG_ERR_INVALID_ARGUMENT);
    cr_assert_eq(ngi_bench_measure(&ng_test_heap()->base, brute, puzzle, &too_many_runs, &result),
                 NG_ERR_INVALID_ARGUMENT);
    cr_assert_eq(
        ngi_bench_measure(&ng_test_heap()->base, brute, puzzle, &too_many_warmups, &result),
        NG_ERR_INVALID_ARGUMENT);
    cr_assert_eq(ng_test_heap()->request_count, 0u, "rejected before any allocation");
    ng_puzzle_destroy(puzzle);
}

Test(bench_measure, the_bounds_of_the_numbers_of_runs_are_accepted)
{
    ng_puzzle *puzzle = puzzle_of(l_rows, l_height);
    ngi_bench_options options = quick_options();
    options.warmup_runs = 0;
    options.measured_runs = 1;
    ngi_bench_result result = {};

    cr_assert_eq(
        ngi_bench_measure(&ng_test_heap()->base, &giving_up_solver, puzzle, &options, &result),
        NG_OK);
    options.warmup_runs = NGI_BENCH_MAX_RUNS;
    options.measured_runs = NGI_BENCH_MAX_RUNS;
    cr_assert_eq(
        ngi_bench_measure(&ng_test_heap()->base, &giving_up_solver, puzzle, &options, &result),
        NG_OK);
    ng_puzzle_destroy(puzzle);
}

/* --- Measurements ---------------------------------------------------------------------- */

Test(bench_measure, brute_force_gives_the_counters_of_a_direct_solve)
{
    ng_puzzle *puzzle = puzzle_of(l_rows, l_height);
    const ng_solver_vtable *brute = ng_solver_find("brute");
    const ngi_bench_options options = quick_options();
    ngi_bench_result result = {};
    ng_grid *grid = nullptr;
    cr_assert_eq(ng_grid_create(&ng_test_heap()->base, 3, l_height, &grid), NG_OK);
    ng_solve_outcome direct = {};
    cr_assert_eq(ng_solve(&ng_test_heap()->base, brute, puzzle, nullptr, grid, &direct), NG_OK);

    cr_assert_eq(ngi_bench_measure(&ng_test_heap()->base, brute, puzzle, &options, &result), NG_OK);

    cr_assert_eq(result.result, NG_SOLVE_SOLVED);
    cr_assert_eq(result.stats.nodes, direct.stats.nodes);
    cr_assert_eq(result.stats.solutions_found, 1u);
    cr_assert_eq(result.stats.workspace_bytes, direct.stats.workspace_bytes);
    cr_assert_eq(result.peak_bytes, direct.stats.workspace_bytes, "the workspace is all S0 uses");
    cr_assert_eq(result.stats.elapsed_ns, result.median_ns);
    cr_assert(result.min_ns <= result.median_ns && result.median_ns <= result.max_ns);
    cr_assert_eq(result.defect, NGI_BENCH_NO_DEFECT);
    ng_grid_destroy(grid);
    ng_puzzle_destroy(puzzle);
}

Test(bench_measure, two_measurements_give_the_same_counters)
{
    ng_puzzle *puzzle = puzzle_of(l_rows, l_height);
    const ng_solver_vtable *brute = ng_solver_find("brute");
    const ngi_bench_options options = quick_options();
    ngi_bench_result first = {};
    ngi_bench_result second = {};

    cr_assert_eq(ngi_bench_measure(&ng_test_heap()->base, brute, puzzle, &options, &first), NG_OK);
    cr_assert_eq(ngi_bench_measure(&ng_test_heap()->base, brute, puzzle, &options, &second), NG_OK);

    cr_assert_eq(first.result, second.result);
    cr_assert_eq(first.stats.nodes, second.stats.nodes);
    cr_assert_eq(first.stats.solutions_found, second.stats.solutions_found);
    cr_assert_eq(first.peak_bytes, second.peak_bytes);
    ng_puzzle_destroy(puzzle);
}

Test(bench_measure, null_options_use_the_defaults)
{
    ng_puzzle *puzzle = puzzle_of(l_rows, l_height);
    ngi_bench_result result = {};
    ng_test_heap()->request_count = 0;

    cr_assert_eq(
        ngi_bench_measure(&ng_test_heap()->base, &giving_up_solver, puzzle, nullptr, &result),
        NG_OK);

    /* times, grid, then one workspace per run: 2 warmup + 11 measured. */
    cr_assert_eq(ng_test_heap()->request_count, 2u + 13u);
    ng_puzzle_destroy(puzzle);
}

Test(bench_measure, a_null_clock_selects_the_monotonic_clock)
{
    ng_puzzle *puzzle = puzzle_of(l_rows, l_height);
    ngi_bench_options options = quick_options();
    options.solve.clock = nullptr;
    ngi_bench_result result = {};

    cr_assert_eq(ngi_bench_measure(&ng_test_heap()->base, ng_solver_find("brute"), puzzle, &options,
                                   &result),
                 NG_OK);

    cr_assert_gt(result.max_ns, 0u, "2^12 candidates take some time on a monotonic clock");
    ng_puzzle_destroy(puzzle);
}

Test(bench_measure, unsolvable_puzzle_is_measured_without_verification)
{
    ng_puzzle *puzzle = read_puzzle(NG_SOURCE_DIR "/tests/data/puzzles/unsolvable/crossed.non");
    ngi_bench_result result = {};

    cr_assert_eq(
        ngi_bench_measure(&ng_test_heap()->base, ng_solver_find("brute"), puzzle, nullptr, &result),
        NG_OK);

    cr_assert_eq(result.result, NG_SOLVE_UNSOLVABLE);
    cr_assert_eq(result.stats.solutions_found, 0u);
    ng_puzzle_destroy(puzzle);
}

Test(bench_measure, median_of_an_odd_number_of_runs_ignores_the_warmup)
{
    static const uint64_t durations[] = {100'000, 90'000, 50, 10, 40, 30, 20};

    const ngi_bench_result result = measure_with_times(durations, 2, 5);

    cr_assert_eq(result.median_ns, 30u);
    cr_assert_eq(result.min_ns, 10u);
    cr_assert_eq(result.max_ns, 50u);
    cr_assert_eq(result.stats.elapsed_ns, 30u);
}

Test(bench_measure, median_of_an_even_number_of_runs_is_the_mean_of_the_middle_ones)
{
    static const uint64_t durations[] = {10, 41, 20, 30};

    const ngi_bench_result result = measure_with_times(durations, 0, 4);

    cr_assert_eq(result.median_ns, 25u, "(20 + 30) / 2; (20 + 41) / 2 would give 30");
    cr_assert_eq(result.min_ns, 10u);
    cr_assert_eq(result.max_ns, 41u);
}

Test(bench_measure, peak_counts_the_solve_and_not_the_grid)
{
    ng_puzzle *puzzle = puzzle_of(l_rows, l_height);
    ngi_bench_result result = {};

    cr_assert_eq(ngi_bench_measure(&ng_test_heap()->base, &giving_up_solver, puzzle,
                                   &(ngi_bench_options){.warmup_runs = 0,
                                                        .measured_runs = 2,
                                                        .solve = ng_solve_options_default()},
                                   &result),
                 NG_OK);

    cr_assert_eq(result.peak_bytes, fake_workspace);
    cr_assert_eq(result.stats.workspace_bytes, fake_workspace);
    cr_assert_eq(result.result, NG_SOLVE_INCOMPLETE);
    cr_assert_eq(result.stats.nodes, 1u);
    ng_puzzle_destroy(puzzle);
}

/* --- Failures -------------------------------------------------------------------------- */

Test(bench_measure, puzzle_beyond_the_solver_is_a_limit_and_out_is_untouched)
{
    ng_puzzle *puzzle = puzzle_of(wide_rows, wide_height);
    ngi_bench_result result = {.median_ns = 7, .defect = NGI_BENCH_INVALID_GRID};

    cr_assert_eq(
        ngi_bench_measure(&ng_test_heap()->base, ng_solver_find("brute"), puzzle, nullptr, &result),
        NG_ERR_LIMIT);

    cr_assert_eq(result.median_ns, 7u);
    cr_assert_eq(result.defect, NGI_BENCH_INVALID_GRID);
    ng_puzzle_destroy(puzzle);
}

Test(bench_measure, invalid_options_of_the_solve_are_reported)
{
    ng_puzzle *puzzle = puzzle_of(l_rows, l_height);
    ngi_bench_options options = quick_options();
    options.solve.max_solutions = 0;
    ngi_bench_result result = {};

    cr_assert_eq(ngi_bench_measure(&ng_test_heap()->base, ng_solver_find("brute"), puzzle, &options,
                                   &result),
                 NG_ERR_INVALID_ARGUMENT);
    ng_puzzle_destroy(puzzle);
}

Test(bench_measure, invalid_solved_grid_is_an_internal_error)
{
    ng_puzzle *puzzle = puzzle_of(l_rows, l_height);
    ngi_bench_result result = {.median_ns = 7};

    cr_assert_eq(ngi_bench_measure(&ng_test_heap()->base, &wrong_solver, puzzle, nullptr, &result),
                 NG_ERR_INTERNAL);

    cr_assert_eq(result.defect, NGI_BENCH_INVALID_GRID);
    cr_assert_eq(result.median_ns, 7u, "only the defect is written");
    ng_puzzle_destroy(puzzle);
}

Test(bench_measure, runs_that_differ_are_an_internal_error)
{
    ng_puzzle *puzzle = puzzle_of(l_rows, l_height);
    ngi_bench_result result = {};

    cr_assert_eq(
        ngi_bench_measure(&ng_test_heap()->base, &drifting_solver, puzzle, nullptr, &result),
        NG_ERR_INTERNAL);

    cr_assert_eq(result.defect, NGI_BENCH_NOT_DETERMINISTIC);
    cr_assert_eq(drifting_nodes, 2u, "detected at the second run");
    ng_puzzle_destroy(puzzle);
}

Test(bench_measure, every_allocation_failure_is_clean)
{
    ng_puzzle *puzzle = puzzle_of(l_rows, l_height);
    const ngi_bench_options options = quick_options();
    ng_test_allocator *ta = ng_test_heap();
    const size_t live_before = ta->live_bytes;
    for (ng_counter n = 0;; ++n) {
        ng_test_allocator_fail_at(ta, n);
        ngi_bench_result result = {.median_ns = 7};

        const ng_status status =
            ngi_bench_measure(&ta->base, ng_solver_find("brute"), puzzle, &options, &result);

        cr_assert_eq(ta->live_bytes, live_before, "request %" PRIu64, n);
        if (status == NG_OK) {
            cr_assert_gt(n, 2u, "times, grid and at least one workspace");
            break;
        }
        cr_assert_eq(status, NG_ERR_OUT_OF_MEMORY, "request %" PRIu64, n);
        cr_assert_eq(result.median_ns, 7u);
    }
    ng_test_allocator_fail_at(ta, NG_TEST_NEVER_FAIL);
    ng_puzzle_destroy(puzzle);
}

/* --- Clock ----------------------------------------------------------------------------- */

Test(bench_clock, readings_convert_to_nanoseconds)
{
    const struct timespec reading = {.tv_sec = 3, .tv_nsec = 250};

    cr_assert_eq(ngi_bench_ns_from_timespec(true, &reading), 3'000'000'250u);
}

Test(bench_clock, failed_or_negative_readings_give_zero)
{
    const struct timespec reading = {.tv_sec = 3, .tv_nsec = 250};
    const struct timespec negative_seconds = {.tv_sec = -1, .tv_nsec = 0};
    const struct timespec negative_nanoseconds = {.tv_sec = 1, .tv_nsec = -1};

    cr_assert_eq(ngi_bench_ns_from_timespec(false, &reading), 0u);
    cr_assert_eq(ngi_bench_ns_from_timespec(true, &negative_seconds), 0u);
    cr_assert_eq(ngi_bench_ns_from_timespec(true, &negative_nanoseconds), 0u);
}

Test(bench_clock, readings_beyond_64_bits_saturate)
{
    const struct timespec seconds_overflow = {.tv_sec = (time_t)(UINT64_MAX / 1'000'000'000u + 1u),
                                              .tv_nsec = 0};
    const struct timespec sum_overflow = {.tv_sec = (time_t)(UINT64_MAX / 1'000'000'000u),
                                          .tv_nsec = 999'999'999};

    cr_assert_eq(ngi_bench_ns_from_timespec(true, &seconds_overflow), UINT64_MAX);
    cr_assert_eq(ngi_bench_ns_from_timespec(true, &sum_overflow), UINT64_MAX);
}

Test(bench_clock, monotonic_clock_never_goes_back)
{
    const ng_clock *clock = ngi_bench_clock();

    const uint64_t first = clock->now_ns(clock->ctx);
    const uint64_t second = clock->now_ns(clock->ctx);

    cr_assert_gt(first, 0u);
    cr_assert_geq(second, first);
}
