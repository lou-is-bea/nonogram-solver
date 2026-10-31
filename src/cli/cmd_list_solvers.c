#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "nonogram/solver.h"

#include "cli.h"

/* nonogram list-solvers: one line per solver of the registry, in its order (F-30):
 * "<name>  complete|incomplete  <summary>", names padded to the same width. */

int cli_list_solvers(int argc, char **argv, FILE *out, FILE *err)
{
    if (argc > 0 && argv[0] != nullptr) {
        return cli_usage_error(err, "list-solvers: unexpected argument", argv[0]);
    }
    int width = 0;
    for (size_t i = 0; i < ng_solver_count(); ++i) {
        const int length = (int)strlen(ng_solver_at(i)->name);
        width = length > width ? length : width;
    }
    for (size_t i = 0; i < ng_solver_count(); ++i) {
        const ng_solver_vtable *solver = ng_solver_at(i);
        (void)fprintf(out, "%-*s  %-10s  %s\n", width, solver->name,
                      solver->complete ? "complete" : "incomplete", solver->summary);
    }
    if (ferror(out) != 0 || fflush(out) != 0) {
        (void)fprintf(err, "nonogram: cannot write the output: %s\n", strerror(errno));
        return CLI_EXIT_INPUT;
    }
    return CLI_EXIT_SUCCESS;
}
