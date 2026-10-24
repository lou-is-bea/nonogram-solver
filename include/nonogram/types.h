/**
 * @file types.h
 * @brief Semantic integer types of the data model, sized for massive grids (ADR-0009).
 *
 * Every dimension, line identifier, cell index or counter of the project uses one of these
 * types, never a bare integer type. Memory sizes stay in size_t.
 */
#ifndef NG_NONOGRAM_TYPES_H
#define NG_NONOGRAM_TYPES_H

#include <inttypes.h>
#include <stdint.h>

/** @brief Width, height, line length, position in a line, run length. At most NG_DIM_MAX. */
typedef uint32_t ng_dim;

/** @brief Unified line identifier: rows are 0..height-1, columns height..height+width-1. */
typedef uint32_t ng_line_id;

/** @brief Number of runs (blocks) of one line. At most (NG_DIM_MAX + 1) / 2. */
typedef uint32_t ng_run_count;

/** @brief Linear cell index or number of cells. At most NG_DIM_MAX squared (< 2^62). */
typedef uint64_t ng_cell_index;

/** @brief Position in the global run storage, or total number of runs of a puzzle. */
typedef uint64_t ng_run_index;

/** @brief Statistics counter (search nodes, line solves). Additive across workers. */
typedef uint64_t ng_counter;

/**
 * @brief Largest representable width or height: 2^31 - 1.
 *
 * A representation bound, not a practical one: memory is the only practical limit.
 */
constexpr ng_dim NG_DIM_MAX = (ng_dim)INT32_MAX;

/** @brief Largest number of runs a line of NG_DIM_MAX cells can hold (runs and gaps). */
constexpr ng_run_count NG_RUN_COUNT_MAX = (ng_run_count)((NG_DIM_MAX / 2u) + 1u);

static_assert((uint64_t)NG_DIM_MAX * 2u <= UINT32_MAX, "line ids must fit in ng_line_id");
static_assert((uint64_t)NG_DIM_MAX * NG_DIM_MAX <= UINT64_MAX / 4u,
              "cell indices must fit in ng_cell_index");
static_assert(NG_RUN_COUNT_MAX * 2ull - 1u == NG_DIM_MAX,
              "a full line alternates NG_RUN_COUNT_MAX runs and their gaps");

/** @name printf/scanf format specifiers for the semantic types. */
/** @{ */
#define NG_PRI_DIM PRIu32
#define NG_PRI_LINE_ID PRIu32
#define NG_PRI_RUN_COUNT PRIu32
#define NG_PRI_CELL_INDEX PRIu64
#define NG_PRI_RUN_INDEX PRIu64
#define NG_PRI_COUNTER PRIu64
/** @} */

#endif /* NG_NONOGRAM_TYPES_H */
