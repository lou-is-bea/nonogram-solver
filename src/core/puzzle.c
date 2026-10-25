#include "nonogram/puzzle.h"

#include <stdckdint.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "nonogram/allocator.h"
#include "nonogram/status.h"
#include "nonogram/types.h"

#include "core/internal/assert.h"
#include "core/internal/puzzle.h"

/* Clues in compressed sparse row form, in a single allocation (ARCHITECTURE.md §4.3):
 * [header][line_offsets: line_count + 1 x ng_run_index][runs: total_runs x ng_dim].
 * The header size is a multiple of alignof(ng_run_index), so both arrays are aligned. */
struct ng_puzzle {
    const ng_allocator *allocator;
    size_t bytes; /* size of the whole allocation, for free() */
    ng_dim width;
    ng_dim height;
    ng_line_id line_count;
    ng_run_index total_runs;
    const ng_run_index *line_offsets;
    const ng_dim *runs;
};
static_assert(sizeof(ng_puzzle) % alignof(ng_run_index) == 0, "offsets follow the header");
static_assert(alignof(ng_run_index) % alignof(ng_dim) == 0, "runs follow the offsets");

static ng_dim length_of(ng_dim width, ng_dim height, ng_line_id line)
{
    return line < height ? width : height;
}

static bool is_line(const ng_puzzle *puzzle, ng_line_id line)
{
    return puzzle != nullptr && line < puzzle->line_count;
}

/* Steps 3 and 4 of the documented validation: offsets well formed, and no line announcing
 * more runs than it can hold. Only the offsets are read. */
static ng_status check_offsets(ng_dim width, ng_dim height, const ng_run_index *offsets)
{
    const ng_line_id line_count = height + width;
    if (offsets[0] != 0u) {
        return NG_ERR_INVALID_ARGUMENT;
    }
    for (ng_line_id line = 0; line < line_count; ++line) {
        if (offsets[line + 1u] < offsets[line]) {
            return NG_ERR_INVALID_ARGUMENT;
        }
    }
    for (ng_line_id line = 0; line < line_count; ++line) {
        /* n cells hold at most (n + 1) / 2 runs: runs of 1 separated by single gaps. */
        const ng_run_index max_runs = ((ng_run_index)length_of(width, height, line) + 1u) / 2u;
        if (offsets[line + 1u] - offsets[line] > max_runs) {
            return NG_ERR_INCONSISTENT;
        }
    }
    return NG_OK;
}

/* Cells needed by the clue of one line: its runs plus one gap between consecutive runs.
 * Accumulated with ckd_add although the run count is already bounded by check_offsets. */
static bool line_need(const ng_dim *runs, ng_run_index begin, ng_run_index end,
                      ng_cell_index *filled, ng_cell_index *need)
{
    ng_cell_index sum = 0;
    for (ng_run_index i = begin; i < end; ++i) {
        if (ckd_add(&sum, sum, (ng_cell_index)runs[i])) {
            return false;
        }
    }
    *filled = sum;
    *need = sum + (end - begin) - (end > begin ? 1u : 0u);
    return true;
}

/* Steps 5 and 6: no empty run, every clue fits its line, equal row and column totals. */
static ng_status check_runs(ng_dim width, ng_dim height, const ng_dim *runs,
                            const ng_run_index *offsets)
{
    const ng_line_id line_count = height + width;
    const ng_run_index total_runs = offsets[line_count];
    for (ng_run_index i = 0; i < total_runs; ++i) {
        if (runs[i] == 0u) {
            return NG_ERR_INVALID_ARGUMENT;
        }
    }
    ng_cell_index row_total = 0;
    ng_cell_index column_total = 0;
    for (ng_line_id line = 0; line < line_count; ++line) {
        ng_cell_index filled = 0;
        ng_cell_index need = 0;
        if (!line_need(runs, offsets[line], offsets[line + 1u], &filled, &need) ||
            need > length_of(width, height, line)) {
            return NG_ERR_INCONSISTENT;
        }
        ng_cell_index *total = line < height ? &row_total : &column_total;
        if (ckd_add(total, *total, filled)) {
            return NG_ERR_INCONSISTENT;
        }
    }
    return row_total == column_total ? NG_OK : NG_ERR_INCONSISTENT;
}

/* Step 7: size of the single allocation. */
static ng_status puzzle_bytes(ng_line_id line_count, ng_run_index total_runs, size_t *bytes)
{
    size_t offsets_bytes = 0;
    size_t runs_bytes = 0;
    if (ckd_mul(&offsets_bytes, (size_t)line_count + 1u, sizeof(ng_run_index)) ||
        ckd_mul(&runs_bytes, total_runs, sizeof(ng_dim)) ||
        ckd_add(bytes, sizeof(ng_puzzle), offsets_bytes) || ckd_add(bytes, *bytes, runs_bytes)) {
        return NG_ERR_LIMIT;
    }
    return NG_OK;
}

static ng_status validate(ng_dim width, ng_dim height, const ng_dim *runs,
                          const ng_run_index *line_offsets)
{
    if (width == 0u || height == 0u || width > NG_DIM_MAX || height > NG_DIM_MAX) {
        return NG_ERR_LIMIT;
    }
    ng_status status = check_offsets(width, height, line_offsets);
    if (status != NG_OK) {
        return status;
    }
    if (line_offsets[height + width] == 0u) {
        return NG_OK; /* no run at all: every clue is empty, both totals are 0 */
    }
    if (runs == nullptr) {
        return NG_ERR_INVALID_ARGUMENT;
    }
    return check_runs(width, height, runs, line_offsets);
}

[[nodiscard]] ng_status ng_puzzle_create(const ng_allocator *allocator, ng_dim width, ng_dim height,
                                         const ng_dim *runs, const ng_run_index *line_offsets,
                                         ng_puzzle **out)
{
    if (out == nullptr) {
        return NG_ERR_INVALID_ARGUMENT;
    }
    *out = nullptr;
    if (allocator == nullptr || line_offsets == nullptr) {
        return NG_ERR_INVALID_ARGUMENT;
    }
    ng_status status = validate(width, height, runs, line_offsets);
    if (status != NG_OK) {
        return status;
    }
    const ng_line_id line_count = height + width;
    const ng_run_index total_runs = line_offsets[line_count];
    size_t bytes = 0;
    status = puzzle_bytes(line_count, total_runs, &bytes);
    if (status != NG_OK) {
        return status;
    }
    unsigned char *block = allocator->alloc(allocator->ctx, bytes, alignof(ng_puzzle));
    if (block == nullptr) {
        return NG_ERR_OUT_OF_MEMORY;
    }
    const size_t offsets_bytes = ((size_t)line_count + 1u) * sizeof(ng_run_index);
    ng_run_index *offsets_copy = (ng_run_index *)(block + sizeof(ng_puzzle));
    ng_dim *runs_copy = (ng_dim *)(block + sizeof(ng_puzzle) + offsets_bytes);
    memcpy(offsets_copy, line_offsets, offsets_bytes);
    if (total_runs != 0u) {
        memcpy(runs_copy, runs, (size_t)total_runs * sizeof(ng_dim));
    }
    ng_puzzle *puzzle = (ng_puzzle *)block;
    *puzzle = (ng_puzzle){.allocator = allocator,
                          .bytes = bytes,
                          .width = width,
                          .height = height,
                          .line_count = line_count,
                          .total_runs = total_runs,
                          .line_offsets = offsets_copy,
                          .runs = runs_copy};
    *out = puzzle;
    return NG_OK;
}

void ng_puzzle_destroy(ng_puzzle *puzzle)
{
    if (puzzle == nullptr) {
        return;
    }
    puzzle->allocator->free(puzzle->allocator->ctx, puzzle, puzzle->bytes);
}

ng_dim ng_puzzle_width(const ng_puzzle *puzzle)
{
    return puzzle == nullptr ? 0u : puzzle->width;
}

ng_dim ng_puzzle_height(const ng_puzzle *puzzle)
{
    return puzzle == nullptr ? 0u : puzzle->height;
}

ng_line_id ng_puzzle_line_count(const ng_puzzle *puzzle)
{
    return puzzle == nullptr ? 0u : puzzle->line_count;
}

ng_run_index ng_puzzle_total_runs(const ng_puzzle *puzzle)
{
    return puzzle == nullptr ? 0u : puzzle->total_runs;
}

ng_dim ng_puzzle_line_length(const ng_puzzle *puzzle, ng_line_id line)
{
    NG_ASSERT(is_line(puzzle, line));
    if (!is_line(puzzle, line)) {
        return 0;
    }
    return length_of(puzzle->width, puzzle->height, line);
}

ng_run_count ng_puzzle_line_run_count(const ng_puzzle *puzzle, ng_line_id line)
{
    NG_ASSERT(is_line(puzzle, line));
    if (!is_line(puzzle, line)) {
        return 0;
    }
    /* Bounded by (line length + 1) / 2 at creation, so it fits in ng_run_count. */
    return (ng_run_count)(puzzle->line_offsets[line + 1u] - puzzle->line_offsets[line]);
}

[[nodiscard]] ng_status ng_puzzle_copy_line_runs(const ng_puzzle *puzzle, ng_line_id line,
                                                 ng_dim *buffer, ng_run_count capacity)
{
    if (!is_line(puzzle, line)) {
        return NG_ERR_INVALID_ARGUMENT;
    }
    const ng_run_count count = ng_puzzle_line_run_count(puzzle, line);
    if (count > capacity || (buffer == nullptr && count != 0u)) {
        return NG_ERR_INVALID_ARGUMENT;
    }
    if (count != 0u) {
        memcpy(buffer, puzzle->runs + puzzle->line_offsets[line], (size_t)count * sizeof(ng_dim));
    }
    return NG_OK;
}

const ng_dim *ngi_puzzle_line_runs(const ng_puzzle *puzzle, ng_line_id line)
{
    NG_ASSERT(is_line(puzzle, line));
    if (ng_puzzle_line_run_count(puzzle, line) == 0u) {
        return nullptr;
    }
    return puzzle->runs + puzzle->line_offsets[line];
}
