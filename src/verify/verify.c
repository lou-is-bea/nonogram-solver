#include "nonogram/verify.h"

#include <stddef.h>
#include <stdint.h>

#include "nonogram/grid.h"
#include "nonogram/puzzle.h"
#include "nonogram/status.h"
#include "nonogram/types.h"

#include "core/internal/puzzle.h"

/* The oracle (ADR-0007): deliberately naive. Every line is scanned cell by cell through the
 * public cell accessor, its runs are recomputed on the fly and compared with the clue. No
 * optimisation, no shared code with the solvers. */

static ng_dim line_length(const ng_grid *grid, ng_line_id line)
{
    return line < ng_grid_height(grid) ? ng_grid_width(grid) : ng_grid_height(grid);
}

static bool is_filled(const ng_grid *grid, ng_line_id line, ng_dim i)
{
    const ng_dim height = ng_grid_height(grid);
    const ng_cell cell =
        line < height ? ng_grid_get(grid, line, i) : ng_grid_get(grid, i, line - height);
    return cell == NG_CELL_FILLED;
}

/* Run boundaries of a line: calls are made in order, with the length of each run. */
typedef struct run_scan {
    const ng_grid *grid;
    ng_line_id line;
    ng_dim next; /* next cell to examine */
} run_scan;

/* Length of the next run of the line, or 0 when there is none left. */
static ng_dim next_run(run_scan *scan)
{
    const ng_dim length = line_length(scan->grid, scan->line);
    while (scan->next < length && !is_filled(scan->grid, scan->line, scan->next)) {
        ++scan->next;
    }
    ng_dim run = 0;
    while (scan->next < length && is_filled(scan->grid, scan->line, scan->next)) {
        ++run;
        ++scan->next;
    }
    return run;
}

static ng_cell_index count_unknown(const ng_grid *grid)
{
    ng_cell_index unknown = 0;
    for (ng_dim row = 0; row < ng_grid_height(grid); ++row) {
        for (ng_dim col = 0; col < ng_grid_width(grid); ++col) {
            if (ng_grid_get(grid, row, col) == NG_CELL_UNKNOWN) {
                ++unknown;
            }
        }
    }
    return unknown;
}

/* True when the runs of @p line differ from its clue; *bad_run is then the index of the first
 * differing run (the shorter count when one list is a prefix of the other). */
static bool line_mismatch(const ng_puzzle *puzzle, const ng_grid *grid, ng_line_id line,
                          ng_run_count *bad_run)
{
    const ng_run_count expected_count = ng_puzzle_line_run_count(puzzle, line);
    const ng_dim *expected = ngi_puzzle_line_runs(puzzle, line);
    run_scan scan = {.grid = grid, .line = line, .next = 0};
    for (ng_run_count k = 0;; ++k) {
        const ng_dim run = next_run(&scan);
        if (run == 0u && k == expected_count) {
            return false; /* both lists end together */
        }
        if (run == 0u || k == expected_count || run != expected[k]) {
            *bad_run = k;
            return true;
        }
    }
}

[[nodiscard]] ng_status ng_verify(const ng_puzzle *puzzle, const ng_grid *grid,
                                  ng_verify_report *report)
{
    if (puzzle == nullptr || grid == nullptr || report == nullptr ||
        ng_grid_width(grid) != ng_puzzle_width(puzzle) ||
        ng_grid_height(grid) != ng_puzzle_height(puzzle)) {
        return NG_ERR_INVALID_ARGUMENT;
    }
    *report = (ng_verify_report){.verdict = NG_VERDICT_VALID};
    report->unknown_cells = count_unknown(grid);
    if (report->unknown_cells != 0u) {
        report->verdict = NG_VERDICT_INCOMPLETE;
        return NG_OK;
    }
    for (ng_line_id line = 0; line < ng_puzzle_line_count(puzzle); ++line) {
        ng_run_count bad_run = 0;
        if (line_mismatch(puzzle, grid, line, &bad_run)) {
            report->verdict = NG_VERDICT_INVALID;
            report->first_bad_line = line;
            report->first_bad_run = bad_run;
            return NG_OK;
        }
    }
    return NG_OK;
}

[[nodiscard]] ng_status ng_verify_line_runs(const ng_grid *grid, ng_line_id line, ng_dim *buffer,
                                            ng_run_count capacity, ng_run_count *count)
{
    if (grid == nullptr || count == nullptr || (buffer == nullptr && capacity != 0u) ||
        (uint64_t)line >= (uint64_t)ng_grid_height(grid) + ng_grid_width(grid)) {
        return NG_ERR_INVALID_ARGUMENT;
    }
    run_scan scan = {.grid = grid, .line = line, .next = 0};
    ng_run_count total = 0;
    for (ng_dim run = next_run(&scan); run != 0u; run = next_run(&scan)) {
        if (total < capacity) {
            buffer[total] = run;
        }
        ++total; /* at most (length + 1) / 2: fits in ng_run_count */
    }
    *count = total;
    return NG_OK;
}
