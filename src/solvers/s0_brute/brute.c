/* Solver S0: binary enumeration of the unknown cells (docs/solvers/s0_brute.md). Deliberately
 * naive, so that it can serve as a reference oracle; it shares no code with the verifier. */
#include "solvers/s0_brute/brute.h"

#include <stdalign.h>
#include <stdckdint.h>
#include <stddef.h>
#include <stdint.h>

#include "nonogram/grid.h"
#include "nonogram/puzzle.h"
#include "nonogram/solver.h"
#include "nonogram/status.h"
#include "nonogram/types.h"

#include "solvers/common/deadline.h"

/* Largest grid S0 accepts (§4): 2^25 candidates at most. The names of the constants and of
 * the state are those of the specification. */
static constexpr ng_cell_index NG_BRUTE_MAX_CELLS = 25;

/* The clock is read every this many nodes only (§5). */
static constexpr ng_counter NGI_BRUTE_TIME_CHECK = 4'096;

/* Written by init, checked by solve: "S0BRUTE1". */
static constexpr uint64_t prepared_marker = UINT64_C(0x5330425255544531);

/* The workspace (§4): this state, then the zones it points to, by decreasing alignment. */
typedef struct ngi_brute_state {
    uint64_t marker;
    ng_dim width;
    ng_dim height;
    ng_cell_index cells;
    const ng_solve_options *options; /* borrowed until solve returns (solver.h) */
    ng_cell_index *unknown;          /* positions of the unknown cells, row-major */
    ng_run_index *offsets;           /* lines + 1 offsets into runs */
    ng_dim *runs;                    /* the clues, line after line */
    ng_cell *candidate;              /* the grid being examined, row-major */
    ng_cell *first;                  /* the first solution found */
} ngi_brute_state;

/* sizeof(ngi_brute_state) rounded up so that the zone after it stays aligned. */
static constexpr size_t state_bytes = (sizeof(ngi_brute_state) + alignof(ng_cell_index) - 1u) /
                                      alignof(ng_cell_index) * alignof(ng_cell_index);

/* Number of cells, or NG_ERR_LIMIT above the limit of S0. */
static ng_status count_cells(const ng_puzzle *puzzle, ng_cell_index *cells)
{
    if (ckd_mul(cells, (ng_cell_index)ng_puzzle_width(puzzle),
                (ng_cell_index)ng_puzzle_height(puzzle)) ||
        *cells > NG_BRUTE_MAX_CELLS) {
        return NG_ERR_LIMIT;
    }
    return NG_OK;
}

/* S + 8 N + 8 (L + 1) + 4 R + 2 N (§4). */
static ng_status layout_size(const ng_puzzle *puzzle, ng_cell_index cells, size_t *out)
{
    const size_t lines = (size_t)ng_puzzle_line_count(puzzle);
    const ng_run_index runs = ng_puzzle_total_runs(puzzle);
    size_t unknown_bytes = 0;
    size_t offset_count = 0;
    size_t offset_bytes = 0;
    size_t run_bytes = 0;
    size_t cell_bytes = 0;
    size_t total = 0;
    if (ckd_mul(&unknown_bytes, (size_t)cells, sizeof(ng_cell_index)) ||
        ckd_add(&offset_count, lines, (size_t)1) ||
        ckd_mul(&offset_bytes, offset_count, sizeof(ng_run_index)) ||
        ckd_mul(&run_bytes, (size_t)runs, sizeof(ng_dim)) ||
        ckd_mul(&cell_bytes, (size_t)cells, 2u * sizeof(ng_cell)) ||
        ckd_add(&total, state_bytes, unknown_bytes) || ckd_add(&total, total, offset_bytes) ||
        ckd_add(&total, total, run_bytes) || ckd_add(&total, total, cell_bytes)) {
        return NG_ERR_LIMIT;
    }
    *out = total;
    return NG_OK;
}

static ng_status brute_workspace_size(const ng_puzzle *puzzle, const ng_solve_options *options,
                                      size_t *out)
{
    if (puzzle == nullptr || options == nullptr || out == nullptr || options->max_solutions == 0u) {
        return NG_ERR_INVALID_ARGUMENT;
    }
    ng_cell_index cells = 0;
    ng_status status = count_cells(puzzle, &cells);
    size_t size = 0;
    if (status == NG_OK) {
        status = layout_size(puzzle, cells, &size);
    }
    if (status != NG_OK) {
        return status;
    }
    if (options->max_memory_bytes != 0u && size > options->max_memory_bytes) {
        return NG_ERR_LIMIT;
    }
    *out = size;
    return NG_OK;
}

static bool is_aligned(const void *workspace)
{
    return (uintptr_t)workspace % alignof(ngi_brute_state) == 0u;
}

/* Points the zones of @p state into the workspace that follows it and copies the clues. */
static ng_status lay_out(ngi_brute_state *state, const ng_puzzle *puzzle)
{
    /* Each zone starts where the previous one ends (§4); the order keeps every zone aligned. */
    const ng_line_id lines = ng_puzzle_line_count(puzzle);
    void *next = (unsigned char *)state + state_bytes;
    state->unknown = next;
    next = state->unknown + state->cells;
    state->offsets = next;
    next = state->offsets + (size_t)lines + 1u;
    state->runs = next;
    next = state->runs + ng_puzzle_total_runs(puzzle);
    state->candidate = next;
    state->first = state->candidate + state->cells;

    ng_run_index position = 0;
    for (ng_line_id line = 0; line < lines; ++line) {
        const ng_run_count count = ng_puzzle_line_run_count(puzzle, line);
        state->offsets[line] = position;
        const ng_status status =
            ng_puzzle_copy_line_runs(puzzle, line, state->runs + position, count);
        if (status != NG_OK) {
            return status;
        }
        position += count;
    }
    state->offsets[lines] = position;
    return NG_OK;
}

static ng_status brute_init(void *workspace, size_t workspace_size, const ng_puzzle *puzzle,
                            const ng_solve_options *options)
{
    /* A failed init must leave a workspace that solve refuses, even a previously prepared one. */
    if (workspace != nullptr && is_aligned(workspace) && workspace_size >= state_bytes) {
        ((ngi_brute_state *)workspace)->marker = 0;
    }
    if (workspace == nullptr || !is_aligned(workspace)) {
        return NG_ERR_INVALID_ARGUMENT;
    }
    size_t needed = 0;
    ng_status status = brute_workspace_size(puzzle, options, &needed);
    if (status != NG_OK) {
        return status;
    }
    if (workspace_size < needed) {
        return NG_ERR_INVALID_ARGUMENT;
    }
    ngi_brute_state *state = workspace;
    *state = (ngi_brute_state){
        .width = ng_puzzle_width(puzzle),
        .height = ng_puzzle_height(puzzle),
        .options = options,
    };
    status = count_cells(puzzle, &state->cells);
    if (status == NG_OK) {
        status = lay_out(state, puzzle);
    }
    if (status != NG_OK) {
        return status;
    }
    state->marker = prepared_marker;
    return NG_OK;
}

/* Whether line @p line of the candidate matches its clue, read in one pass. */
static bool line_matches(const ngi_brute_state *state, ng_line_id line)
{
    const bool is_row = line < state->height;
    const ng_dim length = is_row ? state->width : state->height;
    const ng_cell_index start =
        is_row ? (ng_cell_index)line * state->width : (ng_cell_index)(line - state->height);
    const ng_cell_index step = is_row ? 1u : state->width;
    const ng_dim *expected = state->runs + state->offsets[line];
    const ng_run_index expected_count = state->offsets[line + 1u] - state->offsets[line];
    ng_run_index found = 0;
    ng_dim run = 0;
    for (ng_dim i = 0; i <= length; ++i) {
        const bool filled = i < length && state->candidate[start + i * step] == NG_CELL_FILLED;
        if (filled) {
            ++run;
        } else if (run != 0u) {
            /* found == expected_count comes first: besides rejecting an extra run, it keeps
             * expected[found] inside the clue (memory safety, not only correctness). */
            if (found == expected_count || expected[found] != run) {
                return false;
            }
            ++found;
            run = 0;
        }
    }
    return found == expected_count;
}

/* Rows first, then columns; stops at the first line that does not match (§2). */
static bool candidate_matches(const ngi_brute_state *state)
{
    const ng_line_id lines = state->height + state->width;
    for (ng_line_id line = 0; line < lines; ++line) {
        if (!line_matches(state, line)) {
            return false;
        }
    }
    return true;
}

/* Loads the grid into the candidate, lists the unknown cells and sets them EMPTY. */
static ng_status load(ngi_brute_state *state, const ng_grid *grid, ng_cell_index *unknown_count)
{
    for (ng_dim row = 0; row < state->height; ++row) {
        const ng_status status =
            ng_grid_load_line(grid, row, state->candidate + (ng_cell_index)row * state->width);
        if (status != NG_OK) {
            return status;
        }
    }
    ng_cell_index count = 0;
    for (ng_cell_index cell = 0; cell < state->cells; ++cell) {
        if (state->candidate[cell] == NG_CELL_UNKNOWN) {
            state->unknown[count++] = cell;
            state->candidate[cell] = NG_CELL_EMPTY;
        }
    }
    *unknown_count = count;
    return NG_OK;
}

/* Next candidate in binary order, the last unknown cell being the least significant digit.
 * Returns false once every candidate has been examined. */
static bool next_candidate(ngi_brute_state *state, ng_cell_index unknown_count)
{
    ng_cell_index digit = unknown_count;
    while (digit > 0u && state->candidate[state->unknown[digit - 1u]] == NG_CELL_FILLED) {
        state->candidate[state->unknown[digit - 1u]] = NG_CELL_EMPTY;
        --digit;
    }
    if (digit == 0u) {
        return false;
    }
    state->candidate[state->unknown[digit - 1u]] = NG_CELL_FILLED;
    return true;
}

static void keep_first(ngi_brute_state *state)
{
    for (ng_cell_index cell = 0; cell < state->cells; ++cell) {
        state->first[cell] = state->candidate[cell];
    }
}

typedef struct search_outcome {
    ng_counter nodes;
    uint32_t solutions;
    ng_solve_result result;
} search_outcome;

/* The enumeration loop of §2. */
static search_outcome search(ngi_brute_state *state, ng_cell_index unknown_count)
{
    const ng_solve_options *options = state->options;
    ngi_deadline deadline;
    ngi_deadline_start(&deadline, options);
    search_outcome out = {.result = NG_SOLVE_UNSOLVABLE};
    for (;;) {
        if (options->max_nodes != 0u && out.nodes == options->max_nodes) {
            out.result = NG_SOLVE_LIMIT_REACHED;
            return out;
        }
        if (out.nodes % NGI_BRUTE_TIME_CHECK == 0u && ngi_deadline_passed(&deadline)) {
            out.result = NG_SOLVE_LIMIT_REACHED;
            return out;
        }
        ++out.nodes;
        if (candidate_matches(state)) {
            if (out.solutions == 0u) {
                keep_first(state);
            }
            ++out.solutions;
            if (out.solutions == options->max_solutions) {
                out.result = NG_SOLVE_SOLVED;
                return out;
            }
        }
        if (!next_candidate(state, unknown_count)) {
            out.result = out.solutions > 0u ? NG_SOLVE_SOLVED : NG_SOLVE_UNSOLVABLE;
            return out;
        }
    }
}

static ng_status store_first(const ngi_brute_state *state, ng_grid *grid)
{
    for (ng_dim row = 0; row < state->height; ++row) {
        const ng_status status =
            ng_grid_store_line(grid, row, state->first + (ng_cell_index)row * state->width);
        if (status != NG_OK) {
            return status;
        }
    }
    return NG_OK;
}

static ng_status brute_solve(void *workspace, ng_grid *inout, ng_solve_stats *stats,
                             ng_solve_result *result)
{
    if (workspace == nullptr || inout == nullptr || stats == nullptr || result == nullptr ||
        !is_aligned(workspace)) {
        return NG_ERR_INVALID_ARGUMENT;
    }
    ngi_brute_state *state = workspace;
    if (state->marker != prepared_marker || ng_grid_width(inout) != state->width ||
        ng_grid_height(inout) != state->height) {
        return NG_ERR_INVALID_ARGUMENT;
    }
    ng_cell_index unknown_count = 0;
    ng_status status = load(state, inout, &unknown_count);
    if (status != NG_OK) {
        return status;
    }
    const search_outcome out = search(state, unknown_count);
    if (out.solutions > 0u) {
        status = store_first(state, inout);
        if (status != NG_OK) {
            return status;
        }
    }
    stats->nodes = out.nodes;
    stats->line_solves = 0;
    stats->solutions_found = out.solutions;
    *result = out.result;
    return NG_OK;
}

const ng_solver_vtable ngi_solver_brute = {
    .name = "brute",
    .summary = "S0: brute force over the unknown cells, up to 25 cells (reference oracle)",
    .complete = true,
    .workspace_size = brute_workspace_size,
    .init = brute_init,
    .solve = brute_solve,
};
