#include <criterion/criterion.h>
#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "nonogram/grid.h"
#include "nonogram/puzzle.h"
#include "nonogram/solver.h"
#include "nonogram/status.h"
#include "nonogram/types.h"
#include "nonogram/verify.h"

#include "ng_test_fixture.h"
#include "solvers/s0_brute/brute.h"
#include "test_allocator.h"
#include "test_puzzles.h"

/* Unit tests of S0 (docs/solvers/s0_brute.md §6). Every test stays within 2^20 candidates, so
 * that it also runs under Valgrind (G7); the full tiny corpus is in tests/exhaustive/. */
TestSuite(brute_registry, .init = ng_test_setup, .fini = ng_test_teardown, .timeout = 10);
TestSuite(brute_workspace, .init = ng_test_setup, .fini = ng_test_teardown, .timeout = 10);
TestSuite(brute_init, .init = ng_test_setup, .fini = ng_test_teardown, .timeout = 10);
TestSuite(brute_solve_arguments, .init = ng_test_setup, .fini = ng_test_teardown, .timeout = 10);
TestSuite(brute_solve, .init = ng_test_setup, .fini = ng_test_teardown, .timeout = 10);
TestSuite(brute_limits, .init = ng_test_setup, .fini = ng_test_teardown, .timeout = 10);

/* The limit of the specification (§4); brute.h exposes only the vtable (PLAYBOOKS P1). */
static constexpr ng_cell_index spec_max_cells = 25;

/* --- Helpers -----------------------------------------------------------------------------------
 */

static const ng_allocator *heap(void)
{
    return &ng_test_heap()->base;
}

/* Puzzle whose clues are those of @p rows. */
static ng_puzzle *puzzle_of(const char *const *rows, ng_dim height)
{
    ng_grid *drawing = ng_test_grid_of(rows, height);
    ng_puzzle *puzzle = ng_test_puzzle_from_grid(drawing);
    ng_grid_destroy(drawing);
    return puzzle;
}

/* Puzzle of @p width x @p height whose lines all have an empty clue, built without a grid. */
static ng_puzzle *empty_puzzle(ng_dim width, ng_dim height)
{
    const size_t lines = (size_t)width + height;
    const size_t bytes = (lines + 1u) * sizeof(ng_run_index);
    ng_run_index *offsets = heap()->alloc(heap()->ctx, bytes, alignof(ng_run_index));
    cr_assert_not_null(offsets);
    memset(offsets, 0, bytes);
    ng_puzzle *puzzle = nullptr;
    cr_assert_eq(ng_puzzle_create(heap(), width, height, nullptr, offsets, &puzzle), NG_OK);
    heap()->free(heap()->ctx, offsets, bytes);
    return puzzle;
}

static ng_grid *unknown_grid(ng_dim width, ng_dim height)
{
    ng_grid *grid = nullptr;
    cr_assert_eq(ng_grid_create(heap(), width, height, &grid), NG_OK);
    return grid;
}

static ng_grid *unknown_grid_for(const ng_puzzle *puzzle)
{
    return unknown_grid(ng_puzzle_width(puzzle), ng_puzzle_height(puzzle));
}

static ng_solve_options counting(uint32_t max_solutions)
{
    ng_solve_options options = ng_solve_options_default();
    options.max_solutions = max_solutions;
    return options;
}

static ng_solve_outcome solve(const ng_puzzle *puzzle, ng_grid *grid,
                              const ng_solve_options *options)
{
    ng_solve_outcome outcome = {};
    cr_assert_eq(ng_solve(heap(), &ngi_solver_brute, puzzle, options, grid, &outcome), NG_OK);
    return outcome;
}

static size_t workspace_bytes(const ng_puzzle *puzzle, const ng_solve_options *options)
{
    size_t bytes = 0;
    cr_assert_eq(ngi_solver_brute.workspace_size(puzzle, options, &bytes), NG_OK);
    return bytes;
}

static void assert_grid_is(const ng_grid *grid, const char *const *rows)
{
    for (ng_dim r = 0; r < ng_grid_height(grid); ++r) {
        cr_assert_eq(strlen(rows[r]), (size_t)ng_grid_width(grid), "row %u", r);
        for (ng_dim c = 0; rows[r][c] != '\0'; ++c) {
            const ng_cell expected = rows[r][c] == '#' ? NG_CELL_FILLED : NG_CELL_EMPTY;
            cr_assert_eq(ng_grid_get(grid, r, c), expected, "cell (%u, %u)", r, c);
        }
    }
}

static void assert_all_unknown(const ng_grid *grid)
{
    for (ng_dim r = 0; r < ng_grid_height(grid); ++r) {
        for (ng_dim c = 0; c < ng_grid_width(grid); ++c) {
            cr_assert_eq(ng_grid_get(grid, r, c), NG_CELL_UNKNOWN, "cell (%u, %u)", r, c);
        }
    }
}

static void assert_valid(const ng_puzzle *puzzle, const ng_grid *grid)
{
    ng_verify_report report = {};
    cr_assert_eq(ng_verify(puzzle, grid, &report), NG_OK);
    cr_assert_eq(report.verdict, NG_VERDICT_VALID);
}

/* A workspace allocated on the test heap, for direct calls through the vtable. */
typedef struct workspace {
    void *data;
    size_t size;
} workspace;

static workspace workspace_alloc(size_t size)
{
    workspace ws = {.data = heap()->alloc(heap()->ctx, size, alignof(max_align_t)), .size = size};
    cr_assert_not_null(ws.data);
    memset(ws.data, 0, size); /* a never-prepared workspace reads as zero, not as garbage */
    return ws;
}

static void workspace_free(workspace *ws)
{
    heap()->free(heap()->ctx, ws->data, ws->size);
}

/* The drawings used below. */
static const char *const p_rows[] = {"###..", "#.#..", "###..", "#....", "#...."};
static const char *const checker_rows[] = {"#.#.", ".#.#", "#.#.", ".#.#"};
static const char *const diagonal_rows[] = {"#.", ".#"};
static const char *const permutation_rows[] = {"#...", ".#..", "..#.", "...#"};

/* 3 x 3, unsolvable: rows 1, 0, 1 and columns 0, 2, 0. The column run of 2 needs the empty
 * middle row. (The corpus puzzle unsolvable/crossed is checked by tests/exhaustive/.) */
static ng_puzzle *blocked_puzzle(void)
{
    static const ng_dim runs[] = {1, 1, 2};
    static const ng_run_index offsets[] = {0, 1, 1, 2, 2, 3, 3};
    ng_puzzle *puzzle = nullptr;
    cr_assert_eq(ng_puzzle_create(heap(), 3, 3, runs, offsets, &puzzle), NG_OK);
    return puzzle;
}

/* --- brute_registry ----------------------------------------------------------------------------
 */

Test(brute_registry, s0_is_registered_as_a_complete_solver)
{
    const ng_solver_vtable *solver = ng_solver_find("brute");
    cr_assert_eq(solver, &ngi_solver_brute);
    cr_assert(solver->complete);
    cr_assert_not_null(solver->summary);
}

/* --- brute_workspace ---------------------------------------------------------------------------
 */

/* §4: size = S + 10 N + 8 L + 4 R + 8, with the same S for every puzzle. */
static size_t variable_part(const ng_puzzle *puzzle)
{
    const size_t cells = (size_t)ng_puzzle_width(puzzle) * ng_puzzle_height(puzzle);
    const size_t lines = ng_puzzle_line_count(puzzle);
    const size_t runs = ng_puzzle_total_runs(puzzle);
    return 10u * cells + 8u * lines + 4u * runs + 8u;
}

Test(brute_workspace, size_follows_the_formula_of_the_specification)
{
    static const char *const dot[] = {"#"};
    static const char *const blank[] = {"."};
    static const char *const strip[] = {"#.##.#"};
    ng_puzzle *puzzles[] = {puzzle_of(dot, 1), puzzle_of(blank, 1), puzzle_of(strip, 1),
                            puzzle_of(p_rows, 5), puzzle_of(checker_rows, 4)};
    const ng_solve_options options = ng_solve_options_default();
    const size_t first_fixed = workspace_bytes(puzzles[0], &options) - variable_part(puzzles[0]);

    cr_assert_gt(first_fixed, 0u);
    cr_assert_eq(first_fixed % alignof(ng_cell_index), 0u);
    for (size_t i = 0; i < sizeof puzzles / sizeof puzzles[0]; ++i) {
        const size_t fixed = workspace_bytes(puzzles[i], &options) - variable_part(puzzles[i]);
        cr_assert_eq(fixed, first_fixed, "puzzle %zu", i);
        ng_puzzle_destroy(puzzles[i]);
    }
}

Test(brute_workspace, twenty_five_cells_are_accepted)
{
    ng_puzzle *puzzle = puzzle_of(p_rows, 5);
    const ng_solve_options options = ng_solve_options_default();
    size_t bytes = 0;

    cr_assert_eq((ng_cell_index)ng_puzzle_width(puzzle) * ng_puzzle_height(puzzle), spec_max_cells);
    cr_assert_eq(ngi_solver_brute.workspace_size(puzzle, &options, &bytes), NG_OK);
    ng_puzzle_destroy(puzzle);
}

Test(brute_workspace, more_than_twenty_five_cells_are_refused)
{
    static const struct {
        ng_dim width;
        ng_dim height;
    } shapes[] = {{1, 26}, {26, 1}, {6, 5}, {1, 1'000'000}, {1'000'000, 1}, {70'000, 70'000}};
    const ng_solve_options options = ng_solve_options_default();

    for (size_t i = 0; i < sizeof shapes / sizeof shapes[0]; ++i) {
        ng_puzzle *puzzle = empty_puzzle(shapes[i].width, shapes[i].height);
        size_t bytes = 0;
        cr_assert_eq(ngi_solver_brute.workspace_size(puzzle, &options, &bytes), NG_ERR_LIMIT,
                     "%u x %u", shapes[i].width, shapes[i].height);
        ng_puzzle_destroy(puzzle);
    }
}

Test(brute_workspace, max_memory_below_the_size_is_refused_and_equal_is_accepted)
{
    ng_puzzle *puzzle = puzzle_of(p_rows, 5);
    ng_solve_options options = ng_solve_options_default();
    const size_t exact = workspace_bytes(puzzle, &options);
    size_t bytes = 0;

    options.max_memory_bytes = exact - 1u;
    cr_assert_eq(ngi_solver_brute.workspace_size(puzzle, &options, &bytes), NG_ERR_LIMIT);
    options.max_memory_bytes = exact;
    cr_assert_eq(ngi_solver_brute.workspace_size(puzzle, &options, &bytes), NG_OK);
    cr_assert_eq(bytes, exact);
    ng_puzzle_destroy(puzzle);
}

Test(brute_workspace, invalid_arguments_are_refused)
{
    ng_puzzle *puzzle = puzzle_of(p_rows, 5);
    const ng_solve_options options = ng_solve_options_default();
    const ng_solve_options zero = counting(0);
    size_t bytes = 0;

    cr_assert_eq(ngi_solver_brute.workspace_size(nullptr, &options, &bytes),
                 NG_ERR_INVALID_ARGUMENT);
    cr_assert_eq(ngi_solver_brute.workspace_size(puzzle, nullptr, &bytes), NG_ERR_INVALID_ARGUMENT);
    cr_assert_eq(ngi_solver_brute.workspace_size(puzzle, &options, nullptr),
                 NG_ERR_INVALID_ARGUMENT);
    cr_assert_eq(ngi_solver_brute.workspace_size(puzzle, &zero, &bytes), NG_ERR_INVALID_ARGUMENT);
    ng_puzzle_destroy(puzzle);
}

/* --- brute_init --------------------------------------------------------------------------------
 */

Test(brute_init, invalid_arguments_are_refused)
{
    ng_puzzle *puzzle = puzzle_of(p_rows, 5);
    const ng_solve_options options = ng_solve_options_default();
    const ng_solve_options zero = counting(0);
    workspace ws = workspace_alloc(workspace_bytes(puzzle, &options));

    cr_assert_eq(ngi_solver_brute.init(ws.data, ws.size, nullptr, &options),
                 NG_ERR_INVALID_ARGUMENT);
    cr_assert_eq(ngi_solver_brute.init(ws.data, ws.size, puzzle, nullptr), NG_ERR_INVALID_ARGUMENT);
    cr_assert_eq(ngi_solver_brute.init(ws.data, ws.size, puzzle, &zero), NG_ERR_INVALID_ARGUMENT);
    cr_assert_eq(ngi_solver_brute.init(nullptr, ws.size, puzzle, &options),
                 NG_ERR_INVALID_ARGUMENT);
    workspace_free(&ws);
    ng_puzzle_destroy(puzzle);
}

Test(brute_init, workspace_one_byte_short_or_misaligned_is_refused_and_exact_is_accepted)
{
    ng_puzzle *puzzle = puzzle_of(p_rows, 5);
    const ng_solve_options options = ng_solve_options_default();
    const size_t exact = workspace_bytes(puzzle, &options);
    workspace ws = workspace_alloc(exact + alignof(max_align_t));

    cr_assert_eq(ngi_solver_brute.init(ws.data, exact - 1u, puzzle, &options),
                 NG_ERR_INVALID_ARGUMENT);
    cr_assert_eq(ngi_solver_brute.init((unsigned char *)ws.data + 1, exact, puzzle, &options),
                 NG_ERR_INVALID_ARGUMENT);
    cr_assert_eq(ngi_solver_brute.init(ws.data, exact, puzzle, &options), NG_OK);
    workspace_free(&ws);
    ng_puzzle_destroy(puzzle);
}

Test(brute_init, puzzle_above_the_limit_is_refused)
{
    ng_puzzle *small = puzzle_of(p_rows, 5);
    ng_puzzle *large = empty_puzzle(26, 1);
    const ng_solve_options options = ng_solve_options_default();
    workspace ws = workspace_alloc(workspace_bytes(small, &options) * 4u);

    cr_assert_eq(ngi_solver_brute.init(ws.data, ws.size, large, &options), NG_ERR_LIMIT);
    workspace_free(&ws);
    ng_puzzle_destroy(large);
    ng_puzzle_destroy(small);
}

Test(brute_init, failed_init_leaves_a_workspace_that_solve_refuses)
{
    ng_puzzle *puzzle = puzzle_of(p_rows, 5);
    const ng_solve_options options = ng_solve_options_default();
    const size_t exact = workspace_bytes(puzzle, &options);
    workspace ws = workspace_alloc(exact);
    ng_grid *grid = unknown_grid_for(puzzle);
    ng_solve_stats stats = {};
    ng_solve_result result = NG_SOLVE_INCOMPLETE;

    cr_assert_eq(ngi_solver_brute.init(ws.data, exact, puzzle, &options), NG_OK);
    cr_assert_eq(ngi_solver_brute.init(ws.data, exact - 1u, puzzle, &options),
                 NG_ERR_INVALID_ARGUMENT);
    cr_assert_eq(ngi_solver_brute.solve(ws.data, grid, &stats, &result), NG_ERR_INVALID_ARGUMENT);

    assert_all_unknown(grid);
    cr_assert_eq(result, NG_SOLVE_INCOMPLETE);
    ng_grid_destroy(grid);
    workspace_free(&ws);
    ng_puzzle_destroy(puzzle);
}

/* --- brute_solve_arguments ---------------------------------------------------------------------
 */

Test(brute_solve_arguments, null_arguments_are_refused_and_nothing_is_written)
{
    ng_puzzle *puzzle = puzzle_of(p_rows, 5);
    const ng_solve_options options = ng_solve_options_default();
    workspace ws = workspace_alloc(workspace_bytes(puzzle, &options));
    ng_grid *grid = unknown_grid_for(puzzle);
    ng_solve_stats stats = {.nodes = 77};
    ng_solve_result result = NG_SOLVE_INCOMPLETE;
    cr_assert_eq(ngi_solver_brute.init(ws.data, ws.size, puzzle, &options), NG_OK);

    cr_assert_eq(ngi_solver_brute.solve(nullptr, grid, &stats, &result), NG_ERR_INVALID_ARGUMENT);
    cr_assert_eq(ngi_solver_brute.solve(ws.data, nullptr, &stats, &result),
                 NG_ERR_INVALID_ARGUMENT);
    cr_assert_eq(ngi_solver_brute.solve(ws.data, grid, nullptr, &result), NG_ERR_INVALID_ARGUMENT);
    cr_assert_eq(ngi_solver_brute.solve(ws.data, grid, &stats, nullptr), NG_ERR_INVALID_ARGUMENT);

    assert_all_unknown(grid);
    cr_assert_eq(stats.nodes, 77u);
    cr_assert_eq(result, NG_SOLVE_INCOMPLETE);
    ng_grid_destroy(grid);
    workspace_free(&ws);
    ng_puzzle_destroy(puzzle);
}

Test(brute_solve_arguments, never_prepared_workspace_is_refused)
{
    ng_puzzle *puzzle = puzzle_of(p_rows, 5);
    const ng_solve_options options = ng_solve_options_default();
    workspace ws = workspace_alloc(workspace_bytes(puzzle, &options)); /* zeroed, no init */
    ng_grid *grid = unknown_grid_for(puzzle);
    ng_solve_stats stats = {};
    ng_solve_result result = NG_SOLVE_INCOMPLETE;

    cr_assert_eq(ngi_solver_brute.solve(ws.data, grid, &stats, &result), NG_ERR_INVALID_ARGUMENT);

    assert_all_unknown(grid);
    ng_grid_destroy(grid);
    workspace_free(&ws);
    ng_puzzle_destroy(puzzle);
}

Test(brute_solve_arguments, grid_of_other_dimensions_is_refused_and_left_unchanged)
{
    static const struct {
        ng_dim width;
        ng_dim height;
    } shapes[] = {{6, 5}, {4, 5}, {5, 6}, {5, 4}};
    ng_puzzle *puzzle = puzzle_of(p_rows, 5);
    const ng_solve_options options = ng_solve_options_default();
    workspace ws = workspace_alloc(workspace_bytes(puzzle, &options));
    cr_assert_eq(ngi_solver_brute.init(ws.data, ws.size, puzzle, &options), NG_OK);

    for (size_t i = 0; i < sizeof shapes / sizeof shapes[0]; ++i) {
        ng_grid *grid = unknown_grid(shapes[i].width, shapes[i].height);
        ng_solve_stats stats = {};
        ng_solve_result result = NG_SOLVE_INCOMPLETE;
        cr_assert_eq(ngi_solver_brute.solve(ws.data, grid, &stats, &result),
                     NG_ERR_INVALID_ARGUMENT, "%u x %u", shapes[i].width, shapes[i].height);
        assert_all_unknown(grid);
        cr_assert_eq(stats.nodes, 0u);
        ng_grid_destroy(grid);
    }
    workspace_free(&ws);
    ng_puzzle_destroy(puzzle);
}

/* --- brute_solve: cases of TESTING.md §4.2 -----------------------------------------------------
 */

static void assert_solves_to(const char *const *rows, ng_dim height)
{
    ng_puzzle *puzzle = puzzle_of(rows, height);
    ng_grid *grid = unknown_grid_for(puzzle);
    const ng_solve_options options = counting(2);

    const ng_solve_outcome outcome = solve(puzzle, grid, &options);

    cr_assert_eq(outcome.result, NG_SOLVE_SOLVED);
    cr_assert_eq(outcome.stats.solutions_found, 1u);
    cr_assert_eq(outcome.stats.line_solves, 0u);
    assert_grid_is(grid, rows);
    assert_valid(puzzle, grid);
    ng_grid_destroy(grid);
    ng_puzzle_destroy(puzzle);
}

Test(brute_solve, one_by_one_grid_empty_then_full)
{
    static const char *const empty[] = {"."};
    static const char *const full[] = {"#"};
    assert_solves_to(empty, 1);
    assert_solves_to(full, 1);
}

Test(brute_solve, empty_clue_full_line_and_no_freedom)
{
    static const char *const rows[] = {"#####", ".....", "#.#.#", "##.##"};
    assert_solves_to(rows, 4);
}

Test(brute_solve, very_elongated_grids_in_both_directions)
{
    static const char *const wide[] = {"##.#..###.#.##.#"};
    static const char *const tall[] = {"#", "#", ".", "#", ".", ".", "#", "#",
                                       "#", ".", "#", ".", "#", "#", ".", "#"};
    assert_solves_to(wide, 1);
    assert_solves_to(tall, 16);
}

Test(brute_solve, twenty_five_cells_with_nine_known_cells)
{
    ng_puzzle *puzzle = puzzle_of(p_rows, 5);
    ng_grid *grid = unknown_grid_for(puzzle);
    for (ng_cell_index i = 0; i < 9; ++i) { /* the first nine cells, row-major */
        const ng_dim r = (ng_dim)(i / 5);
        const ng_dim c = (ng_dim)(i % 5);
        ng_grid_set(grid, r, c, p_rows[r][c] == '#' ? NG_CELL_FILLED : NG_CELL_EMPTY);
    }
    const ng_solve_options options = counting(2);

    const ng_solve_outcome outcome = solve(puzzle, grid, &options);

    cr_assert_eq(outcome.result, NG_SOLVE_SOLVED);
    cr_assert_eq(outcome.stats.solutions_found, 1u);
    cr_assert_eq(outcome.stats.nodes, UINT64_C(1) << 16, "full enumeration of 16 unknowns");
    assert_grid_is(grid, p_rows);
    ng_grid_destroy(grid);
    ng_puzzle_destroy(puzzle);
}

Test(brute_solve, multiple_solutions_are_counted_exactly)
{
    static const struct {
        const char *const *rows;
        ng_dim height;
        uint32_t solutions;
    } cases[] = {{checker_rows, 4, 2}, {diagonal_rows, 2, 2}, {permutation_rows, 4, 24}};

    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; ++i) {
        ng_puzzle *puzzle = puzzle_of(cases[i].rows, cases[i].height);
        ng_grid *grid = unknown_grid_for(puzzle);
        const ng_solve_options options = counting(cases[i].solutions + 1u);

        const ng_solve_outcome outcome = solve(puzzle, grid, &options);

        cr_assert_eq(outcome.result, NG_SOLVE_SOLVED, "case %zu", i);
        cr_assert_eq(outcome.stats.solutions_found, cases[i].solutions, "case %zu", i);
        assert_valid(puzzle, grid);
        ng_grid_destroy(grid);
        ng_puzzle_destroy(puzzle);
    }
}

Test(brute_solve, counting_stops_at_max_solutions)
{
    ng_puzzle *puzzle = puzzle_of(permutation_rows, 4);
    ng_grid *grid = unknown_grid_for(puzzle);
    const ng_solve_options options = counting(3);

    const ng_solve_outcome outcome = solve(puzzle, grid, &options);

    cr_assert_eq(outcome.result, NG_SOLVE_SOLVED);
    cr_assert_eq(outcome.stats.solutions_found, 3u);
    cr_assert_lt(outcome.stats.nodes, UINT64_C(1) << 16, "stopped before the end");
    ng_grid_destroy(grid);
    ng_puzzle_destroy(puzzle);
}

Test(brute_solve, first_solution_is_the_smallest_in_enumeration_order)
{
    /* First unknown cell most significant, EMPTY = 0: ".#/#." (0110) comes before "#./.#". */
    static const char *const smallest[] = {".#", "#."};
    ng_puzzle *puzzle = puzzle_of(diagonal_rows, 2);
    ng_grid *grid = unknown_grid_for(puzzle);
    const ng_solve_options options = counting(1);

    const ng_solve_outcome outcome = solve(puzzle, grid, &options);

    cr_assert_eq(outcome.result, NG_SOLVE_SOLVED);
    cr_assert_eq(outcome.stats.nodes, 7u, "candidate 0110 is the seventh");
    assert_grid_is(grid, smallest);
    ng_grid_destroy(grid);
    ng_puzzle_destroy(puzzle);
}

Test(brute_solve, counting_keeps_the_first_solution_in_the_grid)
{
    /* Full enumeration of both solutions: the grid holds the first one found, the smallest. */
    static const char *const smallest[] = {".#", "#."};
    ng_puzzle *puzzle = puzzle_of(diagonal_rows, 2);
    ng_grid *grid = unknown_grid_for(puzzle);
    const ng_solve_options options = counting(3);

    const ng_solve_outcome outcome = solve(puzzle, grid, &options);

    cr_assert_eq(outcome.result, NG_SOLVE_SOLVED);
    cr_assert_eq(outcome.stats.solutions_found, 2u);
    cr_assert_eq(outcome.stats.nodes, 16u);
    assert_grid_is(grid, smallest);
    ng_grid_destroy(grid);
    ng_puzzle_destroy(puzzle);
}

Test(brute_solve, unsolvable_puzzle_enumerates_every_candidate_and_leaves_the_grid)
{
    ng_puzzle *puzzle = blocked_puzzle();
    ng_grid *grid = unknown_grid_for(puzzle);
    const ng_solve_options options = ng_solve_options_default();

    const ng_solve_outcome outcome = solve(puzzle, grid, &options);

    cr_assert_eq(outcome.result, NG_SOLVE_UNSOLVABLE);
    cr_assert_eq(outcome.stats.solutions_found, 0u);
    cr_assert_eq(outcome.stats.nodes, UINT64_C(1) << 9);
    assert_all_unknown(grid);
    ng_grid_destroy(grid);
    ng_puzzle_destroy(puzzle);
}

Test(brute_solve, known_cells_are_kept_and_reduce_the_enumeration)
{
    ng_puzzle *puzzle = blocked_puzzle();
    ng_grid *grid = unknown_grid_for(puzzle);
    ng_grid_set(grid, 0, 0, NG_CELL_EMPTY);
    ng_grid_set(grid, 1, 1, NG_CELL_FILLED);
    const ng_solve_options options = ng_solve_options_default();

    const ng_solve_outcome outcome = solve(puzzle, grid, &options);

    cr_assert_eq(outcome.result, NG_SOLVE_UNSOLVABLE);
    cr_assert_eq(outcome.stats.nodes, UINT64_C(1) << 7);
    cr_assert_eq(ng_grid_get(grid, 0, 0), NG_CELL_EMPTY);
    cr_assert_eq(ng_grid_get(grid, 1, 1), NG_CELL_FILLED);
    ng_grid_destroy(grid);
    ng_puzzle_destroy(puzzle);
}

Test(brute_solve, contradictory_known_cell_gives_unsolvable)
{
    ng_puzzle *puzzle = puzzle_of(p_rows, 5);
    ng_grid *grid = unknown_grid_for(puzzle);
    for (ng_dim c = 0; c < 5; ++c) { /* row 0 must be "###..": fix it, but wrong at (0, 4) */
        ng_grid_set(grid, 0, c, c < 3 || c == 4 ? NG_CELL_FILLED : NG_CELL_EMPTY);
    }
    for (ng_dim c = 0; c < 5; ++c) {
        ng_grid_set(grid, 1, c, p_rows[1][c] == '#' ? NG_CELL_FILLED : NG_CELL_EMPTY);
    }
    const ng_solve_options options = ng_solve_options_default();

    const ng_solve_outcome outcome = solve(puzzle, grid, &options);

    cr_assert_eq(outcome.result, NG_SOLVE_UNSOLVABLE);
    cr_assert_eq(ng_grid_get(grid, 0, 4), NG_CELL_FILLED);
    cr_assert_eq(ng_grid_get(grid, 2, 0), NG_CELL_UNKNOWN);
    ng_grid_destroy(grid);
    ng_puzzle_destroy(puzzle);
}

Test(brute_solve, complete_input_grid_is_one_node)
{
    ng_puzzle *puzzle = puzzle_of(checker_rows, 4);
    ng_grid *grid = ng_test_grid_of(checker_rows, 4);
    const ng_solve_options options = ng_solve_options_default();

    const ng_solve_outcome outcome = solve(puzzle, grid, &options);

    cr_assert_eq(outcome.result, NG_SOLVE_SOLVED);
    cr_assert_eq(outcome.stats.nodes, 1u);
    assert_grid_is(grid, checker_rows);
    ng_grid_destroy(grid);
    ng_puzzle_destroy(puzzle);
}

Test(brute_solve, nothing_is_allocated_during_solve)
{
    ng_puzzle *puzzle = puzzle_of(permutation_rows, 4);
    const ng_solve_options options = counting(100);
    workspace ws = workspace_alloc(workspace_bytes(puzzle, &options));
    ng_grid *grid = unknown_grid_for(puzzle);
    ng_solve_stats stats = {};
    ng_solve_result result = NG_SOLVE_INCOMPLETE;
    cr_assert_eq(ngi_solver_brute.init(ws.data, ws.size, puzzle, &options), NG_OK);
    const ng_counter allocations = ng_test_heap()->alloc_count;

    cr_assert_eq(ngi_solver_brute.solve(ws.data, grid, &stats, &result), NG_OK);

    cr_assert_eq(ng_test_heap()->alloc_count, allocations, "P-S6");
    cr_assert_eq(result, NG_SOLVE_SOLVED);
    cr_assert_eq(stats.solutions_found, 24u);
    ng_grid_destroy(grid);
    workspace_free(&ws);
    ng_puzzle_destroy(puzzle);
}

/* --- brute_limits ------------------------------------------------------------------------------
 */

Test(brute_limits, max_nodes_stops_exactly_and_keeps_the_grid_without_solution)
{
    ng_puzzle *puzzle = blocked_puzzle();
    ng_grid *grid = unknown_grid_for(puzzle);
    ng_solve_options options = ng_solve_options_default();
    options.max_nodes = 100;

    const ng_solve_outcome outcome = solve(puzzle, grid, &options);

    cr_assert_eq(outcome.result, NG_SOLVE_LIMIT_REACHED);
    cr_assert_eq(outcome.stats.nodes, 100u);
    cr_assert_eq(outcome.stats.solutions_found, 0u);
    assert_all_unknown(grid);
    ng_grid_destroy(grid);
    ng_puzzle_destroy(puzzle);
}

Test(brute_limits, max_nodes_during_counting_keeps_the_first_solution)
{
    static const char *const smallest[] = {".#", "#."};
    ng_puzzle *puzzle = puzzle_of(diagonal_rows, 2);
    ng_grid *grid = unknown_grid_for(puzzle);
    ng_solve_options options = counting(3);
    options.max_nodes = 8; /* after the first solution (node 7), before the second (node 10) */

    const ng_solve_outcome outcome = solve(puzzle, grid, &options);

    cr_assert_eq(outcome.result, NG_SOLVE_LIMIT_REACHED);
    cr_assert_eq(outcome.stats.nodes, 8u);
    cr_assert_eq(outcome.stats.solutions_found, 1u, "a lower bound");
    assert_grid_is(grid, smallest);
    ng_grid_destroy(grid);
    ng_puzzle_destroy(puzzle);
}

/* A clock that advances by one millisecond at each reading. */
static uint64_t stepping_now(void *ctx)
{
    uint64_t *now = ctx;
    *now += UINT64_C(1'000'000);
    return *now;
}

Test(brute_limits, max_time_is_checked_every_4096_nodes)
{
    ng_puzzle *puzzle = puzzle_of(permutation_rows, 4);
    ng_grid *grid = unknown_grid_for(puzzle);
    uint64_t now = 0;
    const ng_clock clock = {.now_ns = stepping_now, .ctx = &now};
    ng_solve_options options = counting(100);
    options.clock = &clock;
    options.max_time_ms = 3;

    const ng_solve_outcome outcome = solve(puzzle, grid, &options);

    /* Readings: ng_solve start (1 ms), deadline start (2 ms, end at 5 ms), then one per check at
     * nodes 0 (3 ms), 4096 (4 ms) and 8192 (5 ms: spent). */
    cr_assert_eq(outcome.result, NG_SOLVE_LIMIT_REACHED);
    cr_assert_eq(outcome.stats.nodes, 8192u);
    ng_grid_destroy(grid);
    ng_puzzle_destroy(puzzle);
}

Test(brute_limits, statistics_report_the_workspace_size)
{
    ng_puzzle *puzzle = puzzle_of(checker_rows, 4);
    ng_grid *grid = unknown_grid_for(puzzle);
    const ng_solve_options options = ng_solve_options_default();

    const ng_solve_outcome outcome = solve(puzzle, grid, &options);

    cr_assert_eq(outcome.stats.workspace_bytes, workspace_bytes(puzzle, &options));
    cr_assert_eq(outcome.stats.line_solves, 0u);
    ng_grid_destroy(grid);
    ng_puzzle_destroy(puzzle);
}
