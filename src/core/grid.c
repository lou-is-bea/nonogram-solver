#include "nonogram/grid.h"

#include <stdckdint.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "nonogram/allocator.h"
#include "nonogram/status.h"
#include "nonogram/types.h"

/* One byte per cell, row after row, in the same allocation as the header (ARCHITECTURE.md
 * §4.4). This layout is private: a 2-bit representation can replace it without touching the
 * API. */
struct ng_grid {
    const ng_allocator *allocator;
    size_t bytes; /* size of the whole allocation, for free() */
    ng_dim width;
    ng_dim height;
    ng_cell cells[];
};

static bool is_cell(ng_cell cell)
{
    return cell == NG_CELL_UNKNOWN || cell == NG_CELL_EMPTY || cell == NG_CELL_FILLED;
}

static bool is_inside(const ng_grid *grid, ng_dim row, ng_dim col)
{
    return grid != nullptr && row < grid->height && col < grid->width;
}

static bool is_line(const ng_grid *grid, ng_line_id line)
{
    return (uint64_t)line < (uint64_t)grid->height + grid->width;
}

/* Cannot overflow: row * width + col < NG_DIM_MAX^2, which fits in ng_cell_index (types.h),
 * and the allocation of that many cells succeeded, so the index also fits in size_t. */
static size_t cell_offset(const ng_grid *grid, ng_dim row, ng_dim col)
{
    return (size_t)(((ng_cell_index)row * grid->width) + col);
}

static size_t cell_count(const ng_grid *grid)
{
    return (size_t)((ng_cell_index)grid->width * grid->height);
}

[[nodiscard]] ng_status ng_grid_create(const ng_allocator *allocator, ng_dim width, ng_dim height,
                                       ng_grid **out)
{
    if (out == nullptr) {
        return NG_ERR_INVALID_ARGUMENT;
    }
    *out = nullptr;
    if (allocator == nullptr) {
        return NG_ERR_INVALID_ARGUMENT;
    }
    if (width == 0u || height == 0u || width > NG_DIM_MAX || height > NG_DIM_MAX) {
        return NG_ERR_LIMIT;
    }
    ng_cell_index cells = 0;
    size_t bytes = 0;
    if (ckd_mul(&cells, (ng_cell_index)width, (ng_cell_index)height) ||
        ckd_add(&bytes, sizeof(ng_grid), cells)) {
        return NG_ERR_LIMIT;
    }
    ng_grid *grid = allocator->alloc(allocator->ctx, bytes, alignof(ng_grid));
    if (grid == nullptr) {
        return NG_ERR_OUT_OF_MEMORY;
    }
    *grid = (ng_grid){.allocator = allocator, .bytes = bytes, .width = width, .height = height};
    memset(grid->cells, NG_CELL_UNKNOWN, (size_t)cells);
    *out = grid;
    return NG_OK;
}

void ng_grid_destroy(ng_grid *grid)
{
    if (grid == nullptr) {
        return;
    }
    grid->allocator->free(grid->allocator->ctx, grid, grid->bytes);
}

ng_dim ng_grid_width(const ng_grid *grid)
{
    return grid == nullptr ? 0u : grid->width;
}

ng_dim ng_grid_height(const ng_grid *grid)
{
    return grid == nullptr ? 0u : grid->height;
}

ng_cell ng_grid_get(const ng_grid *grid, ng_dim row, ng_dim col)
{
    if (!is_inside(grid, row, col)) {
        return NG_CELL_UNKNOWN;
    }
    return grid->cells[cell_offset(grid, row, col)];
}

void ng_grid_set(ng_grid *grid, ng_dim row, ng_dim col, ng_cell cell)
{
    if (!is_inside(grid, row, col) || !is_cell(cell)) {
        return;
    }
    grid->cells[cell_offset(grid, row, col)] = cell;
}

void ng_grid_fill(ng_grid *grid, ng_cell cell)
{
    if (grid == nullptr || !is_cell(cell)) {
        return;
    }
    memset(grid->cells, cell, cell_count(grid));
}

[[nodiscard]] ng_status ng_grid_copy(ng_grid *dst, const ng_grid *src)
{
    if (dst == nullptr || src == nullptr || dst->width != src->width ||
        dst->height != src->height) {
        return NG_ERR_INVALID_ARGUMENT;
    }
    if (dst != src) {
        memcpy(dst->cells, src->cells, cell_count(src));
    }
    return NG_OK;
}

[[nodiscard]] ng_status ng_grid_load_line(const ng_grid *grid, ng_line_id line, ng_cell *buffer)
{
    if (grid == nullptr || buffer == nullptr || !is_line(grid, line)) {
        return NG_ERR_INVALID_ARGUMENT;
    }
    if (line < grid->height) {
        memcpy(buffer, grid->cells + cell_offset(grid, line, 0), grid->width);
        return NG_OK;
    }
    /* HOT PATH: called for every line a solver examines; no allocation. */
    const ng_dim col = line - grid->height;
    for (ng_dim row = 0; row < grid->height; ++row) {
        buffer[row] = grid->cells[cell_offset(grid, row, col)];
    }
    return NG_OK;
}

[[nodiscard]] ng_status ng_grid_store_line(ng_grid *grid, ng_line_id line, const ng_cell *buffer)
{
    if (grid == nullptr || buffer == nullptr || !is_line(grid, line)) {
        return NG_ERR_INVALID_ARGUMENT;
    }
    const bool is_row = line < grid->height;
    const ng_dim length = is_row ? grid->width : grid->height;
    /* Validate everything first so that a rejected line leaves the grid unchanged. */
    for (ng_dim i = 0; i < length; ++i) {
        if (!is_cell(buffer[i])) {
            return NG_ERR_INVALID_ARGUMENT;
        }
    }
    if (is_row) {
        memcpy(grid->cells + cell_offset(grid, line, 0), buffer, length);
        return NG_OK;
    }
    /* HOT PATH: called for every line a solver updates; no allocation. */
    const ng_dim col = line - grid->height;
    for (ng_dim row = 0; row < length; ++row) {
        grid->cells[cell_offset(grid, row, col)] = buffer[row];
    }
    return NG_OK;
}
