#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#include "nonogram/solver.h"

static constexpr uint64_t ns_per_s = 1000000000u;

static uint64_t default_now_ns(void *ctx)
{
    (void)ctx;
    struct timespec now = {0};
    if (timespec_get(&now, TIME_UTC) != TIME_UTC || now.tv_sec < 0) {
        return 0;
    }
    /* Saturate rather than wrap: 2^64 ns is about 584 years after the epoch. */
    const uint64_t seconds = (uint64_t)now.tv_sec;
    if (seconds > (UINT64_MAX - (uint64_t)now.tv_nsec) / ns_per_s) {
        return UINT64_MAX;
    }
    return seconds * ns_per_s + (uint64_t)now.tv_nsec;
}

static const ng_clock default_clock = {.now_ns = default_now_ns, .ctx = nullptr};

const ng_clock *ng_clock_default(void)
{
    return &default_clock;
}

ng_solve_options ng_solve_options_default(void)
{
    return (ng_solve_options){.max_solutions = 1};
}

/* The arguments of one ng_solve call, once checked. */
typedef struct solve_call {
    const ng_allocator *allocator;
    const ng_solver_vtable *solver;
    const ng_puzzle *puzzle;
    const ng_solve_options *options;
    ng_grid *inout;
} solve_call;

static bool result_is_known(ng_solve_result result)
{
    switch (result) {
    case NG_SOLVE_SOLVED:
    case NG_SOLVE_UNSOLVABLE:
    case NG_SOLVE_INCOMPLETE:
    case NG_SOLVE_LIMIT_REACHED:
        return true;
    }
    return false;
}

static ng_status check_call(const solve_call *call, const ng_solve_outcome *outcome)
{
    if (call->allocator == nullptr || call->solver == nullptr || call->puzzle == nullptr ||
        call->inout == nullptr || outcome == nullptr) {
        return NG_ERR_INVALID_ARGUMENT;
    }
    const ng_solver_vtable *solver = call->solver;
    if (solver->workspace_size == nullptr || solver->init == nullptr || solver->solve == nullptr) {
        return NG_ERR_INVALID_ARGUMENT;
    }
    if (ng_grid_width(call->inout) != ng_puzzle_width(call->puzzle) ||
        ng_grid_height(call->inout) != ng_puzzle_height(call->puzzle) ||
        call->options->max_solutions == 0u) {
        return NG_ERR_INVALID_ARGUMENT;
    }
    return NG_OK;
}

/* Runs init then solve on an allocated workspace, timed with the clock of the options. */
static ng_status run(const solve_call *call, void *workspace, size_t workspace_size,
                     ng_solve_outcome *outcome)
{
    const ng_clock *clock =
        call->options->clock != nullptr ? call->options->clock : ng_clock_default();
    const uint64_t start = clock->now_ns(clock->ctx);
    ng_status status = call->solver->init(workspace, workspace_size, call->puzzle, call->options);
    if (status == NG_OK) {
        status = call->solver->solve(workspace, call->inout, &outcome->stats, &outcome->result);
    }
    const uint64_t end = clock->now_ns(clock->ctx);
    outcome->stats.elapsed_ns = end >= start ? end - start : 0u;
    return status;
}

ng_status ng_solve(const ng_allocator *allocator, const ng_solver_vtable *solver,
                   const ng_puzzle *puzzle, const ng_solve_options *options, ng_grid *inout,
                   ng_solve_outcome *outcome)
{
    const ng_solve_options defaults = ng_solve_options_default();
    const solve_call call = {
        .allocator = allocator,
        .solver = solver,
        .puzzle = puzzle,
        .options = options != nullptr ? options : &defaults,
        .inout = inout,
    };
    ng_status status = check_call(&call, outcome);
    if (status != NG_OK) {
        return status;
    }

    size_t workspace_size = 0;
    status = solver->workspace_size(puzzle, call.options, &workspace_size);
    if (status != NG_OK) {
        return status;
    }
    if (call.options->max_memory_bytes != 0u && workspace_size > call.options->max_memory_bytes) {
        return NG_ERR_LIMIT;
    }
    void *workspace = nullptr;
    if (workspace_size != 0u) {
        workspace = allocator->alloc(allocator->ctx, workspace_size, alignof(max_align_t));
        if (workspace == nullptr) {
            return NG_ERR_OUT_OF_MEMORY;
        }
    }

    ng_solve_outcome local = {.result = NG_SOLVE_INCOMPLETE};
    status = run(&call, workspace, workspace_size, &local);
    if (workspace != nullptr) {
        allocator->free(allocator->ctx, workspace, workspace_size);
    }
    if (status != NG_OK) {
        return status;
    }
    if (!result_is_known(local.result)) {
        return NG_ERR_INTERNAL; /* broken solver contract */
    }
    local.stats.workspace_bytes = workspace_size; /* the solver does not own this field */
    *outcome = local;
    return NG_OK;
}
