# Solver registry, ng_solve and the solvers themselves. Never link ng_verify here (ADR-0007).
add_library(ng_solvers STATIC
    deadline.c
    registry.c
    solve.c)
# Internal headers (prefix ngi) are reachable as "solvers/internal/..." and "core/internal/...".
target_include_directories(ng_solvers PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(ng_solvers
    PUBLIC ng_core
    PRIVATE ng_project_options)
