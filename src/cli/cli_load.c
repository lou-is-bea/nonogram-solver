#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "nonogram/allocator.h"
#include "nonogram/grid.h"
#include "nonogram/io.h"
#include "nonogram/puzzle.h"
#include "nonogram/status.h"
#include "nonogram/stream.h"
#include "nonogram/types.h"

#include "cli.h"

/* Loading of the input files shared by the subcommands, with their error messages. */

/* Exit code for a library failure while reading @p path. */
static int report_read_error(FILE *err, const char *path, ng_status status,
                             const ng_parse_error *error)
{
    if (status == NG_ERR_OUT_OF_MEMORY || status == NG_ERR_INTERNAL ||
        status == NG_ERR_INVALID_ARGUMENT) {
        (void)fprintf(err, "nonogram: %s: %s\n", path, ng_status_str(status));
        return CLI_EXIT_INTERNAL;
    }
    (void)fprintf(err, "%s:%" PRIu64 ":%" PRIu64 ": error: %s\n", path, error->line, error->column,
                  error->message);
    return CLI_EXIT_INPUT;
}

static FILE *open_input(FILE *err, const char *path)
{
    FILE *file = fopen(path, "rb");
    if (file == nullptr) {
        (void)fprintf(err, "nonogram: cannot open '%s': %s\n", path, strerror(errno));
    }
    return file;
}

int cli_load_puzzle(FILE *err, const char *path, ng_puzzle **puzzle)
{
    FILE *file = open_input(err, path);
    if (file == nullptr) {
        return CLI_EXIT_INPUT;
    }
    const ng_reader r = ng_reader_from_file(file);
    ng_parse_error error = {};
    const ng_status status = ng_io_read_non(ng_allocator_default(), &r, puzzle, nullptr, &error);
    (void)fclose(file); /* read-only: nothing to lose */
    return status == NG_OK ? CLI_EXIT_SUCCESS : report_read_error(err, path, status, &error);
}

int cli_load_grid(FILE *err, const char *path, const ng_puzzle *puzzle, ng_grid **grid)
{
    FILE *file = open_input(err, path);
    if (file == nullptr) {
        return CLI_EXIT_INPUT;
    }
    const ng_reader r = ng_reader_from_file(file);
    ng_parse_error error = {};
    const ng_status status = ng_io_read_sol(ng_allocator_default(), &r, grid, &error);
    (void)fclose(file);
    if (status != NG_OK) {
        return report_read_error(err, path, status, &error);
    }
    if (ng_grid_width(*grid) != ng_puzzle_width(puzzle) ||
        ng_grid_height(*grid) != ng_puzzle_height(puzzle)) {
        (void)fprintf(err,
                      "nonogram: %s: grid is %" NG_PRI_DIM " x %" NG_PRI_DIM
                      ", puzzle is %" NG_PRI_DIM " x %" NG_PRI_DIM "\n",
                      path, ng_grid_width(*grid), ng_grid_height(*grid), ng_puzzle_width(puzzle),
                      ng_puzzle_height(puzzle));
        return CLI_EXIT_INPUT;
    }
    return CLI_EXIT_SUCCESS;
}
