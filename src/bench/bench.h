/**
 * @file bench.h
 * @brief Benchmark harness (module bench, target ng_bench): time and memory of a solver on
 *        one puzzle, measured over several runs (TESTING.md §4.8, NF-10). Internal: used by
 *        "nonogram bench" and the tests, not installed with libnonogram.
 *
 * The harness uses only the public API of libnonogram. Every allocation goes through the
 * caller's allocator, wrapped in a counting allocator that measures the peak.
 */
#ifndef NG_BENCH_BENCH_H
#define NG_BENCH_BENCH_H

#include <stddef.h>
#include <stdint.h>
#include <time.h>

#include "nonogram/allocator.h"
#include "nonogram/puzzle.h"
#include "nonogram/solver.h"
#include "nonogram/status.h"

/** @brief Unmeasured runs before the measured ones, by default (TESTING.md §4.8). */
constexpr uint32_t NGI_BENCH_DEFAULT_WARMUP_RUNS = 2;

/** @brief Measured runs, by default (TESTING.md §4.8): odd, so the median is one run. */
constexpr uint32_t NGI_BENCH_DEFAULT_MEASURED_RUNS = 11;

/** @brief Upper bound of warmup_runs and of measured_runs. */
constexpr uint32_t NGI_BENCH_MAX_RUNS = 1'000;

/** @brief Settings of a measurement. */
typedef struct ngi_bench_options {
    uint32_t warmup_runs;   /**< Runs whose time and memory are ignored, in [0, MAX_RUNS]. */
    uint32_t measured_runs; /**< Runs measured, in [1, MAX_RUNS]. */
    ng_solve_options solve; /**< Options of every run. solve.clock = nullptr selects
                                 ngi_bench_clock(), the monotonic clock. */
} ngi_bench_options;

/**
 * @brief Default settings: 2 warmup runs, 11 measured runs, ng_solve_options_default() with
 *        the monotonic clock.
 * @return The options by value.
 */
ngi_bench_options ngi_bench_options_default(void);

/** @brief Contract breach of a solver, reported with NG_ERR_INTERNAL. */
typedef enum ngi_bench_defect : uint8_t {
    NGI_BENCH_NO_DEFECT,         /**< No breach was detected. */
    NGI_BENCH_INVALID_GRID,      /**< A SOLVED grid is not VALID for ng_verify (ADR-0007). */
    NGI_BENCH_NOT_DETERMINISTIC, /**< Two runs differ in result or counters (NF-12). */
} ngi_bench_defect;

/** @brief What a measurement found. */
typedef struct ngi_bench_result {
    ng_solve_result result;  /**< Result of the runs, the same for every run. */
    ng_solve_stats stats;    /**< Statistics of the runs: the counters are the same for every
                                  run; elapsed_ns is median_ns. */
    uint64_t median_ns;      /**< Median of the measured times (mean of the two middle ones
                                  for an even number of runs). */
    uint64_t min_ns;         /**< Shortest measured time. */
    uint64_t max_ns;         /**< Longest measured time. */
    size_t peak_bytes;       /**< Highest memory a measured run held through the allocator,
                                  above what was allocated before it (the grid, the times). */
    ngi_bench_defect defect; /**< NGI_BENCH_NO_DEFECT, except with NG_ERR_INTERNAL. */
} ngi_bench_result;

/**
 * @brief Run @p solver on @p puzzle from an empty grid, warmup_runs then measured_runs
 *        times, and measure the time (clock of the options) and the peak memory.
 *
 * Every SOLVED grid goes through ng_verify, and every run must give the same result and the
 * same counters (nodes, line solves, solutions, workspace).
 * @param allocator Allocator of the grid, of the times and of the solves. Borrowed, must not
 *                  be nullptr.
 * @param solver    Solver. Borrowed, must not be nullptr.
 * @param puzzle    Puzzle. Borrowed, must not be nullptr.
 * @param options   Settings. Borrowed; nullptr: ngi_bench_options_default().
 * @param out       Receives the measurement. Must not be nullptr. Fully written on NG_OK;
 *                  on NG_ERR_INTERNAL only out->defect is written; untouched otherwise.
 * @return NG_OK; NG_ERR_INVALID_ARGUMENT for a nullptr argument or a number of runs out of
 *         range; NG_ERR_INTERNAL when the solver breaks its contract, with out->defect naming
 *         the check that failed (NGI_BENCH_NO_DEFECT when ng_solve() itself reported it);
 *         NG_ERR_OUT_OF_MEMORY when the allocator refuses; otherwise the error of
 *         ng_grid_create() or ng_solve() (NG_ERR_LIMIT for a puzzle beyond the solver).
 * @ownership Every argument is borrowed; everything allocated is freed on every path.
 * @complexity (warmup_runs + measured_runs) solves; O(width x height + measured_runs) memory
 *             besides the workspace of the solver.
 */
[[nodiscard]] ng_status ngi_bench_measure(const ng_allocator *allocator,
                                          const ng_solver_vtable *solver, const ng_puzzle *puzzle,
                                          const ngi_bench_options *options, ngi_bench_result *out);

/**
 * @brief Monotonic clock of the harness: clock_gettime(CLOCK_MONOTONIC) (POSIX), which
 *        a change of the system time does not affect, unlike ng_clock_default().
 * @return A clock with static storage duration; never nullptr. It keeps no state, and reads
 *         0 if the system clock fails.
 */
const ng_clock *ngi_bench_clock(void);

/**
 * @brief Nanoseconds of a clock reading; exposed for tests.
 * @param valid Whether the reading succeeded.
 * @param now   The reading. Borrowed, must not be nullptr.
 * @return The reading in nanoseconds; 0 when @p valid is false or the reading is negative;
 *         UINT64_MAX when it does not fit in 64 bits.
 */
uint64_t ngi_bench_ns_from_timespec(bool valid, const struct timespec *now);

#endif /* NG_BENCH_BENCH_H */
