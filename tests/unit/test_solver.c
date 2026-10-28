#include <criterion/criterion.h>
#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "nonogram/grid.h"
#include "nonogram/puzzle.h"
#include "nonogram/solver.h"
#include "nonogram/status.h"
#include "nonogram/types.h"

#include "ng_test_fixture.h"
#include "test_allocator.h"
#include "test_puzzles.h"

TestSuite(solver_registry, .init = ng_test_setup, .fini = ng_test_teardown);
TestSuite(solve_options, .init = ng_test_setup, .fini = ng_test_teardown);
TestSuite(solve_clock, .init = ng_test_setup, .fini = ng_test_teardown);
TestSuite(solve_arguments, .init = ng_test_setup, .fini = ng_test_teardown);
TestSuite(solve_workspace, .init = ng_test_setup, .fini = ng_test_teardown);
TestSuite(solve_outcome, .init = ng_test_setup, .fini = ng_test_teardown);

/* --- A fake solver, steered by the test through `fake` ------------------------------------ */

typedef struct fake_solver {
    size_t workspace_bytes; /* returned by workspace_size */
    ng_status size_status;  /* returned by workspace_size */
    ng_status init_status;  /* returned by init */
    ng_status solve_status; /* returned by solve */
    ng_solve_result result; /* written by solve on NG_OK */
    ng_counter nodes;       /* written into the stats by solve */
    /* Observed by the fake functions. */
    void *seen_workspace;
    size_t seen_workspace_size;
    ng_solve_options seen_options;
    bool seen_options_set;
    ng_solve_stats seen_stats; /* stats as solve received them */
    int init_calls;
    int solve_calls;
} fake_solver;

static fake_solver fake;

static ng_status fake_workspace_size(const ng_puzzle *puzzle, const ng_solve_options *options,
                                     size_t *out)
{
    (void)puzzle;
    (void)options;
    *out = fake.workspace_bytes;
    return fake.size_status;
}

static ng_status fake_init(void *workspace, size_t workspace_size, const ng_puzzle *puzzle,
                           const ng_solve_options *options)
{
    (void)puzzle;
    fake.init_calls++;
    fake.seen_workspace = workspace;
    fake.seen_workspace_size = workspace_size;
    fake.seen_options = *options;
    fake.seen_options_set = true;
    if (workspace != nullptr) {
        memset(workspace, 0x5A, workspace_size); /* the whole workspace must be writable */
    }
    return fake.init_status;
}

static ng_status fake_solve(void *workspace, ng_grid *inout, ng_solve_stats *stats,
                            ng_solve_result *result)
{
    (void)workspace;
    (void)inout;
    fake.solve_calls++;
    fake.seen_stats = *stats;
    if (fake.solve_status != NG_OK) {
        return fake.solve_status;
    }
    stats->nodes = fake.nodes;
    stats->solutions_found = fake.result == NG_SOLVE_SOLVED ? 1u : 0u;
    *result = fake.result;
    return NG_OK;
}

static ng_solver_vtable fake_vtable(void)
{
    return (ng_solver_vtable){
        .name = "fake",
        .summary = "test double",
        .complete = true,
        .workspace_size = fake_workspace_size,
        .init = fake_init,
        .solve = fake_solve,
    };
}

static void fake_reset(void)
{
    fake = (fake_solver){
        .workspace_bytes = 64,
        .size_status = NG_OK,
        .init_status = NG_OK,
        .solve_status = NG_OK,
        .result = NG_SOLVE_SOLVED,
        .nodes = 7,
    };
}

/* --- A clock that returns scripted readings ------------------------------------------------ */

typedef struct script_clock {
    const uint64_t *readings;
    size_t count;
    size_t next;
} script_clock;

static uint64_t script_now(void *ctx)
{
    script_clock *clock = ctx;
    const size_t index = clock->next < clock->count ? clock->next : clock->count - 1u;
    clock->next++;
    return clock->readings[index];
}

/* --- Common setup: the "P" of docs/FILE_FORMATS.md, solved by the fake --------------------- */

static const char *const p_rows[] = {"###..", "#.#..", "###..", "#....", "#...."};
static constexpr ng_dim p_height = 5;

typedef struct fixture {
    ng_grid *solution;
    ng_puzzle *puzzle;
    ng_grid *grid;
    ng_solver_vtable solver;
} fixture;

static fixture fixture_create(void)
{
    fake_reset();
    fixture f = {.solver = fake_vtable()};
    f.solution = ng_test_grid_of(p_rows, p_height);
    f.puzzle = ng_test_puzzle_from_grid(f.solution);
    cr_assert_eq(ng_grid_create(&ng_test_heap()->base, 5, 5, &f.grid), NG_OK);
    return f;
}

static void fixture_destroy(fixture *f)
{
    ng_grid_destroy(f->grid);
    ng_puzzle_destroy(f->puzzle);
    ng_grid_destroy(f->solution);
}

/* Sentinels: ng_solve must not write the outputs on error. */
static constexpr ng_counter untouched_nodes = 0xDEADu;
static constexpr ng_solve_result untouched_result = NG_SOLVE_INCOMPLETE;

typedef ng_solve_outcome outputs;

static outputs outputs_untouched(void)
{
    return (outputs){.stats = {.nodes = untouched_nodes}, .result = untouched_result};
}

static void assert_untouched(const outputs *out)
{
    cr_assert_eq(out->stats.nodes, untouched_nodes);
    cr_assert_eq(out->result, untouched_result);
}

static ng_status solve_with(fixture *f, const ng_allocator *allocator,
                            const ng_solve_options *options, outputs *out)
{
    return ng_solve(allocator, &f->solver, f->puzzle, options, f->grid, out);
}

/* --- solver_registry ------------------------------------------------------------------------ */

Test(solver_registry, every_entry_is_complete_and_found_by_its_name)
{
    const size_t count = ng_solver_count();
    for (size_t i = 0; i < count; ++i) {
        const ng_solver_vtable *solver = ng_solver_at(i);
        cr_assert_not_null(solver);
        cr_assert_not_null(solver->name);
        cr_assert_not_null(solver->summary);
        cr_assert_not_null(solver->workspace_size);
        cr_assert_not_null(solver->init);
        cr_assert_not_null(solver->solve);
        cr_assert_eq(ng_solver_find(solver->name), solver, "%s", solver->name);
        for (size_t j = 0; j < i; ++j) {
            cr_assert_str_neq(ng_solver_at(j)->name, solver->name, "duplicate name");
        }
    }
}

Test(solver_registry, index_out_of_range_gives_nullptr)
{
    cr_assert_null(ng_solver_at(ng_solver_count()));
    cr_assert_null(ng_solver_at(SIZE_MAX));
}

Test(solver_registry, unknown_or_null_name_gives_nullptr)
{
    cr_assert_null(ng_solver_find(nullptr));
    cr_assert_null(ng_solver_find(""));
    cr_assert_null(ng_solver_find("no-such-solver"));
}

/* --- solve_options -------------------------------------------------------------------------- */

Test(solve_options, default_has_no_limit_and_stops_at_the_first_solution)
{
    const ng_solve_options options = ng_solve_options_default();

    cr_assert_eq(options.max_nodes, 0u);
    cr_assert_eq(options.max_time_ms, 0u);
    cr_assert_eq(options.max_memory_bytes, 0u);
    cr_assert_eq(options.max_solutions, 1u);
    cr_assert_eq(options.seed, 0u);
    cr_assert_null(options.clock);
}

/* --- solve_clock ---------------------------------------------------------------------------- */

Test(solve_clock, default_clock_exists_and_does_not_go_backwards_across_reads)
{
    const ng_clock *clock = ng_clock_default();
    cr_assert_not_null(clock);
    cr_assert_not_null(clock->now_ns);

    const uint64_t first = clock->now_ns(clock->ctx);
    const uint64_t second = clock->now_ns(clock->ctx);

    cr_assert_gt(first, 0u);
    cr_assert_geq(second, first);
}

/* --- solve_arguments ------------------------------------------------------------------------ */

Test(solve_arguments, null_pointer_is_rejected_without_writing_outputs)
{
    fixture f = fixture_create();
    const ng_allocator *heap = &ng_test_heap()->base;
    outputs out = outputs_untouched();
    const ng_counter before = ng_test_heap()->alloc_count;

    cr_assert_eq(ng_solve(nullptr, &f.solver, f.puzzle, nullptr, f.grid, &out),
                 NG_ERR_INVALID_ARGUMENT);
    cr_assert_eq(ng_solve(heap, nullptr, f.puzzle, nullptr, f.grid, &out), NG_ERR_INVALID_ARGUMENT);
    cr_assert_eq(ng_solve(heap, &f.solver, nullptr, nullptr, f.grid, &out),
                 NG_ERR_INVALID_ARGUMENT);
    cr_assert_eq(ng_solve(heap, &f.solver, f.puzzle, nullptr, nullptr, &out),
                 NG_ERR_INVALID_ARGUMENT);
    cr_assert_eq(ng_solve(heap, &f.solver, f.puzzle, nullptr, f.grid, nullptr),
                 NG_ERR_INVALID_ARGUMENT);

    assert_untouched(&out);
    cr_assert_eq(fake.init_calls, 0);
    cr_assert_eq(ng_test_heap()->alloc_count, before);
    fixture_destroy(&f);
}

Test(solve_arguments, incomplete_vtable_is_rejected)
{
    fixture f = fixture_create();
    outputs out = outputs_untouched();
    const ng_solver_vtable full = f.solver;

    f.solver.workspace_size = nullptr;
    cr_assert_eq(solve_with(&f, &ng_test_heap()->base, nullptr, &out), NG_ERR_INVALID_ARGUMENT);
    f.solver = full;
    f.solver.init = nullptr;
    cr_assert_eq(solve_with(&f, &ng_test_heap()->base, nullptr, &out), NG_ERR_INVALID_ARGUMENT);
    f.solver = full;
    f.solver.solve = nullptr;
    cr_assert_eq(solve_with(&f, &ng_test_heap()->base, nullptr, &out), NG_ERR_INVALID_ARGUMENT);

    assert_untouched(&out);
    fixture_destroy(&f);
}

Test(solve_arguments, grid_of_other_dimensions_is_rejected)
{
    fixture f = fixture_create();
    outputs out = outputs_untouched();
    ng_grid *narrow = nullptr;
    ng_grid *short_grid = nullptr;
    cr_assert_eq(ng_grid_create(&ng_test_heap()->base, 4, 5, &narrow), NG_OK);
    cr_assert_eq(ng_grid_create(&ng_test_heap()->base, 5, 4, &short_grid), NG_OK);

    cr_assert_eq(ng_solve(&ng_test_heap()->base, &f.solver, f.puzzle, nullptr, narrow, &out),
                 NG_ERR_INVALID_ARGUMENT);
    cr_assert_eq(ng_solve(&ng_test_heap()->base, &f.solver, f.puzzle, nullptr, short_grid, &out),
                 NG_ERR_INVALID_ARGUMENT);

    assert_untouched(&out);
    cr_assert_eq(fake.init_calls, 0);
    ng_grid_destroy(short_grid);
    ng_grid_destroy(narrow);
    fixture_destroy(&f);
}

Test(solve_arguments, zero_max_solutions_is_rejected)
{
    fixture f = fixture_create();
    outputs out = outputs_untouched();
    ng_solve_options options = ng_solve_options_default();
    options.max_solutions = 0;

    cr_assert_eq(solve_with(&f, &ng_test_heap()->base, &options, &out), NG_ERR_INVALID_ARGUMENT);

    assert_untouched(&out);
    cr_assert_eq(fake.init_calls, 0);
    fixture_destroy(&f);
}

/* --- solve_workspace ------------------------------------------------------------------------ */

Test(solve_workspace, size_error_is_returned_and_nothing_is_allocated)
{
    fixture f = fixture_create();
    outputs out = outputs_untouched();
    const ng_counter before = ng_test_heap()->alloc_count;
    fake.size_status = NG_ERR_LIMIT;

    cr_assert_eq(solve_with(&f, &ng_test_heap()->base, nullptr, &out), NG_ERR_LIMIT);

    assert_untouched(&out);
    cr_assert_eq(ng_test_heap()->alloc_count, before);
    cr_assert_eq(fake.init_calls, 0);
    fixture_destroy(&f);
}

Test(solve_workspace, size_above_max_memory_is_refused_even_if_the_solver_accepts_it)
{
    fixture f = fixture_create();
    outputs out = outputs_untouched();
    const ng_counter before = ng_test_heap()->alloc_count;
    ng_solve_options options = ng_solve_options_default();
    options.max_memory_bytes = fake.workspace_bytes - 1u;

    cr_assert_eq(solve_with(&f, &ng_test_heap()->base, &options, &out), NG_ERR_LIMIT);

    assert_untouched(&out);
    cr_assert_eq(ng_test_heap()->alloc_count, before);
    fixture_destroy(&f);
}

Test(solve_workspace, size_equal_to_max_memory_is_accepted)
{
    fixture f = fixture_create();
    outputs out = outputs_untouched();
    ng_solve_options options = ng_solve_options_default();
    options.max_memory_bytes = fake.workspace_bytes;

    cr_assert_eq(solve_with(&f, &ng_test_heap()->base, &options, &out), NG_OK);
    cr_assert_eq(out.stats.workspace_bytes, fake.workspace_bytes);
    fixture_destroy(&f);
}

Test(solve_workspace, workspace_has_the_requested_size_and_maximal_alignment)
{
    fixture f = fixture_create();
    outputs out = outputs_untouched();
    fake.workspace_bytes = 1000;

    cr_assert_eq(solve_with(&f, &ng_test_heap()->base, nullptr, &out), NG_OK);

    cr_assert_not_null(fake.seen_workspace);
    cr_assert_eq(fake.seen_workspace_size, 1000u);
    cr_assert_eq((uintptr_t)fake.seen_workspace % alignof(max_align_t), 0u);
    cr_assert_eq(ng_test_heap()->peak_bytes >= 1000u, true);
    cr_assert_eq(ng_test_heap()->misuse_count, 0u);
    fixture_destroy(&f); /* the teardown fails the test if the workspace leaked */
}

Test(solve_workspace, empty_workspace_is_not_allocated)
{
    fixture f = fixture_create();
    outputs out = outputs_untouched();
    const ng_counter before = ng_test_heap()->alloc_count;
    fake.workspace_bytes = 0;

    cr_assert_eq(solve_with(&f, &ng_test_heap()->base, nullptr, &out), NG_OK);

    cr_assert_null(fake.seen_workspace);
    cr_assert_eq(fake.seen_workspace_size, 0u);
    cr_assert_eq(ng_test_heap()->alloc_count, before);
    cr_assert_eq(out.stats.workspace_bytes, 0u);
    fixture_destroy(&f);
}

Test(solve_workspace, allocation_failure_gives_out_of_memory)
{
    fixture f = fixture_create();
    outputs out = outputs_untouched();
    ng_test_allocator_fail_at(ng_test_heap(), 0);

    cr_assert_eq(solve_with(&f, &ng_test_heap()->base, nullptr, &out), NG_ERR_OUT_OF_MEMORY);

    assert_untouched(&out);
    cr_assert_eq(fake.init_calls, 0);
    ng_test_allocator_fail_at(ng_test_heap(), NG_TEST_NEVER_FAIL);
    fixture_destroy(&f);
}

Test(solve_workspace, init_error_is_returned_and_the_workspace_freed)
{
    fixture f = fixture_create();
    outputs out = outputs_untouched();
    const size_t live_before = ng_test_heap()->live_bytes;
    fake.init_status = NG_ERR_INTERNAL;

    cr_assert_eq(solve_with(&f, &ng_test_heap()->base, nullptr, &out), NG_ERR_INTERNAL);

    assert_untouched(&out);
    cr_assert_eq(fake.solve_calls, 0);
    cr_assert_eq(ng_test_heap()->live_bytes, live_before);
    fixture_destroy(&f);
}

Test(solve_workspace, solve_error_is_returned_and_the_workspace_freed)
{
    fixture f = fixture_create();
    outputs out = outputs_untouched();
    const size_t live_before = ng_test_heap()->live_bytes;
    fake.solve_status = NG_ERR_LIMIT;

    cr_assert_eq(solve_with(&f, &ng_test_heap()->base, nullptr, &out), NG_ERR_LIMIT);

    assert_untouched(&out);
    cr_assert_eq(ng_test_heap()->live_bytes, live_before);
    fixture_destroy(&f);
}

/* --- solve_outcome -------------------------------------------------------------------------- */

Test(solve_outcome, result_and_solver_statistics_are_returned)
{
    fixture f = fixture_create();
    outputs out = outputs_untouched();
    fake.result = NG_SOLVE_UNSOLVABLE;
    fake.nodes = 42;

    cr_assert_eq(solve_with(&f, &ng_test_heap()->base, nullptr, &out), NG_OK);

    cr_assert_eq(out.result, NG_SOLVE_UNSOLVABLE);
    cr_assert_eq(out.stats.nodes, 42u);
    cr_assert_eq(out.stats.solutions_found, 0u);
    cr_assert_eq(out.stats.workspace_bytes, fake.workspace_bytes);
    cr_assert_eq(fake.init_calls, 1);
    cr_assert_eq(fake.solve_calls, 1);
    fixture_destroy(&f);
}

Test(solve_outcome, solver_receives_zeroed_statistics)
{
    fixture f = fixture_create();
    outputs out = outputs_untouched();
    out.stats.line_solves = 99;

    cr_assert_eq(solve_with(&f, &ng_test_heap()->base, nullptr, &out), NG_OK);

    cr_assert_eq(fake.seen_stats.nodes, 0u);
    cr_assert_eq(fake.seen_stats.line_solves, 0u);
    cr_assert_eq(fake.seen_stats.solutions_found, 0u);
    cr_assert_eq(out.stats.line_solves, 0u);
    fixture_destroy(&f);
}

Test(solve_outcome, null_options_mean_the_default_options)
{
    fixture f = fixture_create();
    outputs out = outputs_untouched();

    cr_assert_eq(solve_with(&f, &ng_test_heap()->base, nullptr, &out), NG_OK);

    cr_assert(fake.seen_options_set);
    const ng_solve_options expected = ng_solve_options_default();
    cr_assert_eq(fake.seen_options.max_nodes, expected.max_nodes);
    cr_assert_eq(fake.seen_options.max_time_ms, expected.max_time_ms);
    cr_assert_eq(fake.seen_options.max_memory_bytes, expected.max_memory_bytes);
    cr_assert_eq(fake.seen_options.max_solutions, expected.max_solutions);
    cr_assert_eq(fake.seen_options.seed, expected.seed);
    fixture_destroy(&f);
}

Test(solve_outcome, options_are_passed_to_the_solver)
{
    fixture f = fixture_create();
    outputs out = outputs_untouched();
    ng_solve_options options = ng_solve_options_default();
    options.max_nodes = 11;
    options.max_time_ms = 22;
    options.max_solutions = 33;
    options.seed = 44;

    cr_assert_eq(solve_with(&f, &ng_test_heap()->base, &options, &out), NG_OK);

    cr_assert_eq(fake.seen_options.max_nodes, 11u);
    cr_assert_eq(fake.seen_options.max_time_ms, 22u);
    cr_assert_eq(fake.seen_options.max_solutions, 33u);
    cr_assert_eq(fake.seen_options.seed, 44u);
    fixture_destroy(&f);
}

Test(solve_outcome, elapsed_time_comes_from_the_given_clock)
{
    fixture f = fixture_create();
    outputs out = outputs_untouched();
    static const uint64_t readings[] = {1000, 1250};
    script_clock script = {.readings = readings, .count = 2};
    const ng_clock clock = {.now_ns = script_now, .ctx = &script};
    ng_solve_options options = ng_solve_options_default();
    options.clock = &clock;

    cr_assert_eq(solve_with(&f, &ng_test_heap()->base, &options, &out), NG_OK);

    cr_assert_eq(out.stats.elapsed_ns, 250u);
    cr_assert_eq(script.next, 2u);
    fixture_destroy(&f);
}

Test(solve_outcome, clock_going_backwards_gives_zero_elapsed_time)
{
    fixture f = fixture_create();
    outputs out = outputs_untouched();
    static const uint64_t readings[] = {5000, 4000};
    script_clock script = {.readings = readings, .count = 2};
    const ng_clock clock = {.now_ns = script_now, .ctx = &script};
    ng_solve_options options = ng_solve_options_default();
    options.clock = &clock;

    cr_assert_eq(solve_with(&f, &ng_test_heap()->base, &options, &out), NG_OK);

    cr_assert_eq(out.stats.elapsed_ns, 0u);
    fixture_destroy(&f);
}

Test(solve_outcome, result_outside_the_enumeration_is_an_internal_error)
{
    fixture f = fixture_create();
    outputs out = outputs_untouched();
    const size_t live_before = ng_test_heap()->live_bytes;
    /* A broken solver writes a value outside the enumeration; memcpy stores that byte without
     * an out-of-range enum conversion in the test itself. */
    const uint8_t broken = (uint8_t)NG_SOLVE_LIMIT_REACHED + 1u;
    static_assert(sizeof fake.result == sizeof broken);
    memcpy(&fake.result, &broken, sizeof broken);

    cr_assert_eq(solve_with(&f, &ng_test_heap()->base, nullptr, &out), NG_ERR_INTERNAL);

    assert_untouched(&out);
    cr_assert_eq(ng_test_heap()->live_bytes, live_before);
    fixture_destroy(&f);
}
