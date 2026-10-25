/**
 * @file render.h
 * @brief Text rendering of a puzzle and of a complete or partial grid (F-10 to F-13).
 *
 * Layout (ASCII, clues shown):
 *
 *         1
 *       3 1 3 1 1
 *     +-----------+
 *   3 | # # # . . |
 * 1 1 | # . # . . |
 *   3 | # # # . . |
 *   1 | # . . . . |
 *   1 | # . . . . |
 *     +-----------+
 *
 * - Column clues are stacked above the grid, bottom-aligned; row clues are right-aligned on
 *   the left, numbers separated by one space. An empty clue is written "0".
 * - Every column is as wide as the widest column clue number, plus one separating space;
 *   symbols and numbers are right-aligned in it.
 * - Cells: ASCII '#' filled, '.' empty, '?' unknown (as in .sol); Unicode U+2588 filled,
 *   U+00B7 empty, '?' unknown, with a box-drawing frame. Every glyph is one column wide.
 * - No line ends with a space; every line ends with '\n'.
 *
 * The rendering is meant for human-sized grids: the CLI switches to ng_render_summary() above
 * a configurable size (--max-render).
 */
#ifndef NG_NONOGRAM_RENDER_H
#define NG_NONOGRAM_RENDER_H

#include "nonogram/grid.h"
#include "nonogram/puzzle.h"
#include "nonogram/status.h"
#include "nonogram/stream.h"

/** @brief Rendering options. A zero-initialised value means: no clues, ASCII. */
typedef struct ng_render_options {
    bool show_clues; /**< Draw the row and column clues around the grid. */
    bool unicode;    /**< Unicode glyphs and frame instead of ASCII. */
} ng_render_options;

/**
 * @brief Render a puzzle and a grid as text.
 *
 * @param puzzle  Puzzle: dimensions and clues. Must not be nullptr.
 * @param grid    Cells to draw, same dimensions as @p puzzle; nullptr draws every cell as
 *                unknown.
 * @param options Options; nullptr means clues shown, ASCII.
 * @param writer  Destination. Must not be nullptr.
 * @return NG_OK; NG_ERR_INVALID_ARGUMENT for a nullptr puzzle or writer, or a grid whose
 *         dimensions differ from the puzzle's (nothing is written then); otherwise the first
 *         error of @p writer.
 * @ownership Every argument is borrowed. Nothing is allocated.
 * @complexity O(width x height + number of runs) time, O(1) memory.
 */
[[nodiscard]] ng_status ng_render_text(const ng_puzzle *puzzle, const ng_grid *grid,
                                       const ng_render_options *options, const ng_writer *writer);

/**
 * @brief Write a short summary instead of the full rendering, for large grids.
 *
 * Two lines at most:
 *
 *     puzzle: 1000 x 800, 1800 lines, 52000 runs, 400000 filled cells
 *     grid: 400000 filled, 399000 empty, 1000 unknown
 *
 * @param puzzle Puzzle. Must not be nullptr.
 * @param grid   Grid, same dimensions as @p puzzle, or nullptr (no "grid" line).
 * @param writer Destination. Must not be nullptr.
 * @return Same as ng_render_text().
 * @ownership Every argument is borrowed. Nothing is allocated.
 * @complexity O(width x height + height) time, O(1) memory.
 */
[[nodiscard]] ng_status ng_render_summary(const ng_puzzle *puzzle, const ng_grid *grid,
                                          const ng_writer *writer);

#endif /* NG_NONOGRAM_RENDER_H */
