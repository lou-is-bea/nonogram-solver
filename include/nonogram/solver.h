/**
 * @file solver.h
 * @brief Common solver interface, registry and ng_solve (ARCHITECTURE.md §6, F-30 to F-35).
 *
 * A solver is a table of three functions. The caller asks for the size of its workspace,
 * provides that memory, then calls init and solve; neither allocates (NF-04). ng_solve does
 * all of this with an injectable allocator. The registry lists the solvers of the library by
 * name; it is a constant table, so it can be read from several threads at once.
 */
#ifndef NG_NONOGRAM_SOLVER_H
#define NG_NONOGRAM_SOLVER_H

#include <stddef.h>
#include <stdint.h>

#include "nonogram/allocator.h"
#include "nonogram/grid.h"
#include "nonogram/puzzle.h"
#include "nonogram/status.h"
#include "nonogram/types.h"

/** @brief Outcome of a solve that ran without error (F-31). */
enum ng_solve_result : uint8_t {
    NG_SOLVE_SOLVED = 0,    /**< The grid is complete and satisfies every clue. */
    NG_SOLVE_UNSOLVABLE,    /**< No grid satisfies the clues and the known cells. */
    NG_SOLVE_INCOMPLETE,    /**< The solver is stuck; the grid holds its sure deductions. */
    NG_SOLVE_LIMIT_REACHED, /**< A node or time limit stopped the search. */
};
typedef enum ng_solve_result ng_solve_result;

/**
 * @brief Source of time for elapsed_ns and max_time_ms.
 *
 * Injectable like the allocator, so that tests can drive time and the library keeps no global
 * state. A clock should not go backwards; when it does, the library clamps the elapsed time
 * to 0 instead of reporting negative time.
 */
typedef struct ng_clock {
    /** Nanoseconds since an arbitrary origin. Must not be nullptr. */
    uint64_t (*now_ns)(void *ctx);
    /** Passed to now_ns; owned by whoever owns the clock. */
    void *ctx;
} ng_clock;

/**
 * @brief Default clock, based on timespec_get(TIME_UTC) (C23).
 *
 * Wall-clock time: a change of the system time during a solve distorts elapsed_ns and
 * max_time_ms. A decrease is clamped, never reported as negative time.
 *
 * @return A clock with static storage duration; never nullptr. It keeps no state.
 */
const ng_clock *ng_clock_default(void);

/** @brief Limits and settings of a solve. A zero limit means "no limit". */
typedef struct ng_solve_options {
    ng_counter max_nodes;    /**< Search nodes; 0: unlimited. What a node is depends on the
                                  solver (see its specification in docs/solvers/). */
    uint64_t max_time_ms;    /**< Time budget of solve, in milliseconds, enforced by the
                                  solver; 0: unlimited. */
    size_t max_memory_bytes; /**< Workspace size; 0: unlimited. */
    uint32_t max_solutions;  /**< 1: stop at the first solution; > 1: count up to this
                                  number (F-35). Must not be 0. */
    uint64_t seed;           /**< Seed of any randomness (NF-12). */
    const ng_clock *clock;   /**< Borrowed; nullptr: ng_clock_default(). */
} ng_solve_options;

/**
 * @brief Default options: no limit, stop at the first solution, seed 0, default clock.
 * @return The options by value.
 */
ng_solve_options ng_solve_options_default(void);

/** @brief Statistics of a solve (F-34). */
typedef struct ng_solve_stats {
    ng_counter nodes;         /**< Search nodes visited (solver). */
    ng_counter line_solves;   /**< Calls to a line solver (solver; 0 if it has none). */
    uint64_t elapsed_ns;      /**< Time of init + solve (ng_solve). */
    size_t workspace_bytes;   /**< Workspace allocated before init (ng_solve). */
    uint32_t solutions_found; /**< Solutions found (solver); see the result for exactness. */
} ng_solve_stats;

/** @brief What ng_solve returns on success: the result and the statistics. */
typedef struct ng_solve_outcome {
    ng_solve_result result; /**< Outcome of the search. */
    ng_solve_stats stats;   /**< Statistics of the solver and of ng_solve. */
} ng_solve_outcome;

/**
 * @brief Interface of a solver (ARCHITECTURE.md §6).
 *
 * Contract: workspace_size is pure and returns the total memory needed, or NG_ERR_LIMIT when
 * it exceeds max_memory_bytes or is not representable. init and solve never allocate.
 * solve never turns a known cell into NG_CELL_UNKNOWN, leaves a complete grid on
 * NG_SOLVE_SOLVED, accesses the grid by lines or cells only, uses no recursion and is
 * deterministic for a given seed. solve fills nodes, line_solves and solutions_found of
 * @p stats, which the caller zeroes first. The options passed to init, and the clock they
 * point to, stay valid until solve returns. A solver may be called directly through this
 * table, without ng_solve: it then validates its own arguments.
 */
typedef struct ng_solver_vtable {
    const char *name;    /**< Registry name, e.g. "brute". */
    const char *summary; /**< One line, for list-solvers. */
    bool complete;       /**< Always finds a solution when one exists, without limits. */
    /**
     * Workspace size for @p puzzle and @p options, written to @p out.
     * Returns NG_OK, NG_ERR_LIMIT (too large, or above max_memory_bytes), or
     * NG_ERR_INVALID_ARGUMENT. Pure: no allocation, no side effect.
     */
    ng_status (*workspace_size)(const ng_puzzle *puzzle, const ng_solve_options *options,
                                size_t *out);
    /**
     * Prepare @p workspace (@p workspace_size bytes, aligned on alignof(max_align_t); nullptr
     * when the size is 0) for @p puzzle. Everything is borrowed until solve returns.
     * Returns NG_OK or an error (NG_ERR_INVALID_ARGUMENT for a workspace that does not fit).
     */
    ng_status (*init)(void *workspace, size_t workspace_size, const ng_puzzle *puzzle,
                      const ng_solve_options *options);
    /**
     * Search from the cells of @p inout and write *@p result. Returns NG_OK, or an error,
     * in which case *@p result is meaningless and @p inout may hold sure deductions.
     */
    ng_status (*solve)(void *workspace, ng_grid *inout, ng_solve_stats *stats,
                       ng_solve_result *result);
} ng_solver_vtable;

/**
 * @brief Find a solver of the registry by name.
 * @param name Registry name; may be nullptr. Borrowed.
 * @return The solver, or nullptr when @p name is nullptr or unknown.
 * @ownership The returned table has static storage duration; never free it.
 * @complexity O(number of solvers), no allocation.
 */
const ng_solver_vtable *ng_solver_find(const char *name);

/**
 * @brief Number of solvers in the registry.
 * @return The number of solvers; ng_solver_at() accepts every index below it.
 * @complexity O(number of solvers), no allocation.
 */
size_t ng_solver_count(void);

/**
 * @brief Solver at a position of the registry, in a fixed order.
 * @param index Position, below ng_solver_count().
 * @return The solver, or nullptr when @p index is out of range.
 * @ownership The returned table has static storage duration; never free it.
 * @complexity O(number of solvers), no allocation.
 */
const ng_solver_vtable *ng_solver_at(size_t index);

/**
 * @brief Solve a puzzle: size the workspace, allocate it, init, solve, free.
 *
 * @param allocator Allocator of the workspace. Borrowed, must not be nullptr.
 * @param solver    Solver, from the registry or not. Borrowed, must not be nullptr, and its
 *                  three functions must not be nullptr.
 * @param puzzle    Puzzle. Borrowed, must not be nullptr.
 * @param options   Options, borrowed; nullptr: ng_solve_options_default().
 * @param inout     Starting grid, possibly partly known, same dimensions as @p puzzle.
 *                  Updated in place according to the result.
 * @param outcome   Receives the result and the statistics. Must not be nullptr. Written only
 *                  on NG_OK.
 * @return NG_OK (the outcome is in @p outcome); NG_ERR_INVALID_ARGUMENT for a nullptr
 *         argument, an incomplete vtable, a clock without now_ns, mismatched dimensions or
 *         max_solutions = 0; NG_ERR_LIMIT when the workspace exceeds max_memory_bytes or is
 *         not representable; NG_ERR_OUT_OF_MEMORY when the allocator refuses;
 *         NG_ERR_INTERNAL when the solver breaks its contract (returns NG_OK without a
 *         result of the enumeration); otherwise the error of the solver. On error,
 *         @p inout may hold deductions already made by solve.
 * @ownership Every argument is borrowed. The workspace is freed before returning, on every
 *            path.
 * @complexity That of the solver, plus one allocation.
 */
[[nodiscard]] ng_status ng_solve(const ng_allocator *allocator, const ng_solver_vtable *solver,
                                 const ng_puzzle *puzzle, const ng_solve_options *options,
                                 ng_grid *inout, ng_solve_outcome *outcome);

#endif /* NG_NONOGRAM_SOLVER_H */
