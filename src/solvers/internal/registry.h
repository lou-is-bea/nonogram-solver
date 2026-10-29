/**
 * @file registry.h
 * @brief Lookups in a nullptr-terminated list of solvers. Internal.
 *
 * The public functions of solver.h apply them to the registry of the library; tests apply
 * them to their own lists, so that the lookups are covered whatever the registry holds.
 */
#ifndef NG_SOLVERS_INTERNAL_REGISTRY_H
#define NG_SOLVERS_INTERNAL_REGISTRY_H

#include <stddef.h>

#include "nonogram/solver.h"

/** @brief Number of entries of @p list, which ends with nullptr. */
size_t ngi_registry_count(const ng_solver_vtable *const *list);

/** @brief Entry @p index of @p list, or nullptr when @p index is out of range. */
const ng_solver_vtable *ngi_registry_at(const ng_solver_vtable *const *list, size_t index);

/** @brief First entry of @p list named @p name, or nullptr (also for a nullptr @p name). */
const ng_solver_vtable *ngi_registry_find(const ng_solver_vtable *const *list, const char *name);

#endif /* NG_SOLVERS_INTERNAL_REGISTRY_H */
