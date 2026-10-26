/**
 * @file grid.h
 * @brief Grid: the state of every cell of a puzzle, complete or partial. Opaque.
 *
 * The storage is a detail of the library (one byte per cell today, two bits later). Code
 * outside the library reads and writes cells one at a time or, preferably, through line copies
 * (ng_grid_load_line / ng_grid_store_line), which keep working whatever the storage
 * (ADR-0009).
 *
 * Line identifiers are unified: 0..height-1 are the rows (top to bottom), height..
 * height+width-1 the columns (left to right). Rows are read left to right, columns top to
 * bottom.
 */
#ifndef NG_NONOGRAM_GRID_H
#define NG_NONOGRAM_GRID_H

#include <stdint.h>

#include "nonogram/allocator.h"
#include "nonogram/status.h"
#include "nonogram/types.h"

/** @brief State of one cell. The values are also the 2-bit encoding of the .ngg format. */
enum ng_cell : uint8_t {
    NG_CELL_UNKNOWN = 0, /**< Not decided yet. */
    NG_CELL_EMPTY = 1,   /**< White. */
    NG_CELL_FILLED = 2,  /**< Black. */
};
typedef enum ng_cell ng_cell;

/** @brief A width x height grid of cells. Opaque. */
typedef struct ng_grid ng_grid;

/**
 * @brief Create a grid whose cells are all NG_CELL_UNKNOWN.
 *
 * @param allocator Allocator for the grid. Borrowed: must outlive the grid. Must not be
 *                  nullptr.
 * @param width     Number of columns, in [1, NG_DIM_MAX].
 * @param height    Number of rows, in [1, NG_DIM_MAX].
 * @param out       Receives the grid on NG_OK, nullptr on failure. Must not be nullptr.
 * @return NG_OK; NG_ERR_INVALID_ARGUMENT for a nullptr argument; NG_ERR_LIMIT for a dimension
 *         out of range or a size that is not representable; NG_ERR_OUT_OF_MEMORY when the
 *         allocator fails.
 * @ownership The caller owns *out and releases it with ng_grid_destroy().
 * @complexity O(width * height) time; one allocation of width * height bytes plus a small
 *             header.
 */
[[nodiscard]] ng_status ng_grid_create(const ng_allocator *allocator, ng_dim width, ng_dim height,
                                       ng_grid **out);

/**
 * @brief Destroy a grid.
 *
 * @param grid Grid to destroy, or nullptr (no-op). Invalid afterwards.
 * @ownership Takes back ownership of @p grid.
 * @complexity O(1).
 */
void ng_grid_destroy(ng_grid *grid);

/** @brief Number of columns of @p grid; 0 for nullptr. O(1). */
ng_dim ng_grid_width(const ng_grid *grid);

/** @brief Number of rows of @p grid; 0 for nullptr. O(1). */
ng_dim ng_grid_height(const ng_grid *grid);

/**
 * @brief Read one cell.
 *
 * This accessor cannot report an error: a nullptr grid or a position outside the grid yields
 * NG_CELL_UNKNOWN, in every build.
 *
 * @param grid Grid.
 * @param row  Row, below ng_grid_height().
 * @param col  Column, below ng_grid_width().
 * @return The state of the cell, or NG_CELL_UNKNOWN for invalid arguments.
 * @complexity O(1).
 */
ng_cell ng_grid_get(const ng_grid *grid, ng_dim row, ng_dim col);

/**
 * @brief Write one cell.
 *
 * This function cannot report an error: a call with a nullptr grid, a position outside the grid
 * or a value that is not an ng_cell enumerator changes nothing, in every build.
 *
 * @param grid Grid; a nullptr grid is ignored.
 * @param row  Row, below ng_grid_height().
 * @param col  Column, below ng_grid_width().
 * @param cell New state.
 * @complexity O(1).
 */
void ng_grid_set(ng_grid *grid, ng_dim row, ng_dim col, ng_cell cell);

/**
 * @brief Set every cell to @p cell.
 *
 * Same invalid arguments as ng_grid_set(): the call then changes nothing.
 *
 * @param grid Grid; a nullptr grid is ignored.
 * @param cell New state of every cell.
 * @complexity O(width * height).
 */
void ng_grid_fill(ng_grid *grid, ng_cell cell);

/**
 * @brief Copy every cell of @p src into @p dst.
 *
 * @param dst Destination, same dimensions as @p src. Must not be nullptr.
 * @param src Source. Must not be nullptr. May be @p dst (no-op).
 * @return NG_OK; NG_ERR_INVALID_ARGUMENT for a nullptr argument or different dimensions
 *         (@p dst is then unchanged).
 * @complexity O(width * height).
 */
[[nodiscard]] ng_status ng_grid_copy(ng_grid *dst, const ng_grid *src);

/**
 * @brief Copy one line of the grid into a caller buffer.
 *
 * @param grid   Grid. Must not be nullptr.
 * @param line   Unified line identifier, below height + width.
 * @param buffer Receives the line: width cells for a row, height cells for a column.
 *               Must not be nullptr.
 * @return NG_OK; NG_ERR_INVALID_ARGUMENT for a nullptr argument or a line out of range
 *         (@p buffer is then unchanged).
 * @ownership @p buffer is borrowed; it typically lives in a solver workspace.
 * @complexity O(line length) time, no allocation.
 */
[[nodiscard]] ng_status ng_grid_load_line(const ng_grid *grid, ng_line_id line, ng_cell *buffer);

/**
 * @brief Overwrite one line of the grid from a caller buffer.
 *
 * The buffer is validated before anything is written: on error the grid is unchanged.
 *
 * @param grid   Grid. Must not be nullptr.
 * @param line   Unified line identifier, below height + width.
 * @param buffer The new line: width cells for a row, height cells for a column, each an
 *               ng_cell enumerator. Must not be nullptr.
 * @return NG_OK; NG_ERR_INVALID_ARGUMENT for a nullptr argument, a line out of range or a
 *         value that is not an ng_cell enumerator.
 * @ownership @p buffer is borrowed.
 * @complexity O(line length) time, no allocation.
 */
[[nodiscard]] ng_status ng_grid_store_line(ng_grid *grid, ng_line_id line, const ng_cell *buffer);

#endif /* NG_NONOGRAM_GRID_H */
