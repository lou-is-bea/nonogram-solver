#include "bench.h"

#include <stdalign.h>
#include <stdckdint.h>
#include <stddef.h>
#include <stdint.h>

#include "nonogram/allocator.h"
#include "nonogram/grid.h"
#include "nonogram/puzzle.h"
#include "nonogram/solver.h"
#include "nonogram/status.h"
#include "nonogram/verify.h"

/* Wraps the caller's allocator to measure the memory held: every allocation of a
 * measurement goes through it, so none escapes the count. */
typedef struct counting_allocator {
    ng_allocator base;
    const ng_allocator *inner;
    size_t live_bytes;
    size_t peak_bytes;
} counting_allocator;

static void *counting_alloc(void *ctx, size_t size, size_t align)
{
    counting_allocator *counter = ctx;
    void *block = counter->inner->alloc(counter->inner->ctx, size, align);
    if (block != nullptr) {
        /* Live blocks cannot outgrow the address space; saturate all the same. */
        if (ckd_add(&counter->live_bytes, counter->live_bytes, size)) {
            counter->live_bytes = SIZE_MAX;
        }
        if (counter->live_bytes > counter->peak_bytes) {
            counter->peak_bytes = counter->live_bytes;
        }
    }
    return block;
}

static void counting_free(void *ctx, void *ptr, size_t size)
{
    counting_allocator *counter = ctx;
    if (ptr != nullptr) {
        counter->live_bytes = counter->live_bytes >= size ? counter->live_bytes - size : 0u;
    }
    counter->inner->free(counter->inner->ctx, ptr, size);
}

ngi_bench_options ngi_bench_options_default(void)
{
    ngi_bench_options options = {
        .warmup_runs = NGI_BENCH_DEFAULT_WARMUP_RUNS,
        .measured_runs = NGI_BENCH_DEFAULT_MEASURED_RUNS,
        .solve = ng_solve_options_default(),
    };
    options.solve.clock = ngi_bench_clock();
    return options;
}

/* What every run shares. */
typedef struct bench_run {
    counting_allocator *counter;
    const ng_solver_vtable *solver;
    const ng_puzzle *puzzle;
    const ng_solve_options *solve;
    ng_grid *grid;
} bench_run;

/* One solve from an empty grid; *peak receives the memory it held above what was live
 * before it. */
static ng_status run_once(const bench_run *run, ng_solve_outcome *outcome, size_t *peak,
                          ngi_bench_defect *defect)
{
    ng_grid_fill(run->grid, NG_CELL_UNKNOWN);
    const size_t live_before = run->counter->live_bytes;
    run->counter->peak_bytes = live_before;
    const ng_status status =
        ng_solve(&run->counter->base, run->solver, run->puzzle, run->solve, run->grid, outcome);
    *peak = run->counter->peak_bytes - live_before;
    if (status != NG_OK) {
        return status;
    }
    if (outcome->result == NG_SOLVE_SOLVED) {
        ng_verify_report report = {};
        if (ng_verify(run->puzzle, run->grid, &report) != NG_OK ||
            report.verdict != NG_VERDICT_VALID) {
            *defect = NGI_BENCH_INVALID_GRID;
            return NG_ERR_INTERNAL;
        }
    }
    return NG_OK;
}

/* Runs are deterministic (NF-12): everything but the time must repeat. */
static bool same_run(const ng_solve_outcome *a, const ng_solve_outcome *b)
{
    return a->result == b->result && a->stats.nodes == b->stats.nodes &&
           a->stats.line_solves == b->stats.line_solves &&
           a->stats.solutions_found == b->stats.solutions_found &&
           a->stats.workspace_bytes == b->stats.workspace_bytes;
}

/* Sorts the @p count measured times and derives the median, the minimum and the maximum.
 * Insertion sort: at most NGI_BENCH_MAX_RUNS values, sorted once, outside any measurement. */
static void summarise(uint64_t *times, uint32_t count, ngi_bench_result *result)
{
    for (uint32_t i = 1; i < count; ++i) {
        const uint64_t value = times[i];
        uint32_t j = i;
        for (; j > 0u && times[j - 1u] > value; --j) {
            times[j] = times[j - 1u];
        }
        times[j] = value;
    }
    const uint32_t middle = count / 2u;
    if (count % 2u != 0u) {
        result->median_ns = times[middle];
    } else {
        const uint64_t low = times[middle - 1u];
        result->median_ns = low + ((times[middle] - low) / 2u);
    }
    result->min_ns = times[0];
    result->max_ns = times[count - 1u];
}

static ng_status measure_runs(const bench_run *run, const ngi_bench_options *options,
                              uint64_t *times, ngi_bench_result *result)
{
    ng_solve_outcome first = {};
    size_t peak = 0;
    const uint32_t total = options->warmup_runs + options->measured_runs; /* <= 2 x MAX_RUNS */
    for (uint32_t i = 0; i < total; ++i) {
        ng_solve_outcome outcome = {};
        size_t run_peak = 0;
        const ng_status status = run_once(run, &outcome, &run_peak, &result->defect);
        if (status != NG_OK) {
            return status;
        }
        if (i == 0u) {
            first = outcome;
        } else if (!same_run(&first, &outcome)) {
            result->defect = NGI_BENCH_NOT_DETERMINISTIC;
            return NG_ERR_INTERNAL;
        }
        if (i >= options->warmup_runs) {
            times[i - options->warmup_runs] = outcome.stats.elapsed_ns;
            peak = run_peak > peak ? run_peak : peak;
        }
    }
    result->result = first.result;
    result->stats = first.stats;
    result->peak_bytes = peak;
    summarise(times, options->measured_runs, result);
    result->stats.elapsed_ns = result->median_ns;
    return NG_OK;
}

ng_status ngi_bench_measure(const ng_allocator *allocator, const ng_solver_vtable *solver,
                            const ng_puzzle *puzzle, const ngi_bench_options *options,
                            ngi_bench_result *out)
{
    if (allocator == nullptr || solver == nullptr || puzzle == nullptr || out == nullptr) {
        return NG_ERR_INVALID_ARGUMENT;
    }
    const ngi_bench_options defaults = ngi_bench_options_default();
    const ngi_bench_options *o = options != nullptr ? options : &defaults;
    if (o->warmup_runs > NGI_BENCH_MAX_RUNS || o->measured_runs == 0u ||
        o->measured_runs > NGI_BENCH_MAX_RUNS) {
        return NG_ERR_INVALID_ARGUMENT;
    }
    ng_solve_options solve = o->solve;
    if (solve.clock == nullptr) {
        solve.clock = ngi_bench_clock();
    }
    counting_allocator counter = {
        .base = {.alloc = counting_alloc, .free = counting_free, .ctx = nullptr},
        .inner = allocator,
    };
    counter.base.ctx = &counter;

    ngi_bench_result result = {};
    ng_grid *grid = nullptr;
    size_t times_bytes = 0;
    uint64_t *times = nullptr;
    ng_status status = NG_ERR_OUT_OF_MEMORY;
    if (ckd_mul(&times_bytes, (size_t)o->measured_runs, sizeof *times)) {
        status = NG_ERR_LIMIT;
        goto cleanup;
    }
    times = counter.base.alloc(counter.base.ctx, times_bytes, alignof(uint64_t));
    if (times == nullptr) {
        goto cleanup;
    }
    status =
        ng_grid_create(&counter.base, ng_puzzle_width(puzzle), ng_puzzle_height(puzzle), &grid);
    if (status == NG_OK) {
        const bench_run run = {
            .counter = &counter, .solver = solver, .puzzle = puzzle, .solve = &solve, .grid = grid};
        status = measure_runs(&run, o, times, &result);
    }

cleanup:
    ng_grid_destroy(grid);
    counter.base.free(counter.base.ctx, times, times_bytes);
    if (status == NG_OK) {
        *out = result;
    } else if (status == NG_ERR_INTERNAL) {
        out->defect = result.defect;
    }
    return status;
}
