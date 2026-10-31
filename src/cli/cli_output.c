#include <stddef.h>
#include <stdio.h>

#include "nonogram/solver.h"

#include "cli.h"

/* What solve and bench share: the choice of the solver and the wording of the outputs. */

const ng_solver_vtable *cli_choose_solver(const char *name)
{
    if (name != nullptr) {
        return ng_solver_find(name);
    }
    for (size_t i = 0; i < ng_solver_count(); ++i) {
        const ng_solver_vtable *solver = ng_solver_at(i);
        if (solver->complete) {
            return solver;
        }
    }
    return nullptr;
}

const char *cli_result_text(ng_solve_result result)
{
    switch (result) {
    case NG_SOLVE_SOLVED:
        return "solved";
    case NG_SOLVE_UNSOLVABLE:
        return "unsolvable";
    case NG_SOLVE_INCOMPLETE:
        return "incomplete";
    case NG_SOLVE_LIMIT_REACHED:
        return "limit reached";
    }
    return "unknown";
}

const char *cli_result_json(ng_solve_result result)
{
    switch (result) {
    case NG_SOLVE_SOLVED:
        return "SOLVED";
    case NG_SOLVE_UNSOLVABLE:
        return "UNSOLVABLE";
    case NG_SOLVE_INCOMPLETE:
        return "INCOMPLETE";
    case NG_SOLVE_LIMIT_REACHED:
        return "LIMIT_REACHED";
    }
    return "UNKNOWN";
}

void cli_print_json_string(FILE *out, const char *text)
{
    (void)fputc('"', out);
    for (const unsigned char *c = (const unsigned char *)text; *c != '\0'; ++c) {
        if (*c == '"' || *c == '\\') {
            (void)fprintf(out, "\\%c", *c);
        } else if (*c < (unsigned char)' ') {
            (void)fprintf(out, "\\u%04x", (unsigned)*c);
        } else {
            (void)fputc(*c, out);
        }
    }
    (void)fputc('"', out);
}
