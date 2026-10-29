#include "solvers/internal/registry.h"

#include <stddef.h>
#include <string.h>

#include "nonogram/solver.h"

/* The solvers of the library, in the order of list-solvers, terminated by nullptr. Adding a
 * solver adds one line here (NF-11). The list is scanned rather than counted at compile time,
 * so that an empty registry stays valid C without always-false comparisons. */
static const ng_solver_vtable *const registry[] = {
    nullptr,
};

size_t ngi_registry_count(const ng_solver_vtable *const *list)
{
    size_t count = 0;
    while (list[count] != nullptr) {
        ++count;
    }
    return count;
}

const ng_solver_vtable *ngi_registry_at(const ng_solver_vtable *const *list, size_t index)
{
    for (size_t i = 0; list[i] != nullptr; ++i) {
        if (i == index) {
            return list[i];
        }
    }
    return nullptr;
}

const ng_solver_vtable *ngi_registry_find(const ng_solver_vtable *const *list, const char *name)
{
    if (name == nullptr) {
        return nullptr;
    }
    for (size_t i = 0; list[i] != nullptr; ++i) {
        if (strcmp(list[i]->name, name) == 0) {
            return list[i];
        }
    }
    return nullptr;
}

size_t ng_solver_count(void)
{
    return ngi_registry_count(registry);
}

const ng_solver_vtable *ng_solver_at(size_t index)
{
    return ngi_registry_at(registry, index);
}

const ng_solver_vtable *ng_solver_find(const char *name)
{
    return ngi_registry_find(registry, name);
}
