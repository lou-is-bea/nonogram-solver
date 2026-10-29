# Solver registry, ng_solve and the solvers themselves. Never link ng_verify here (ADR-0007).
add_library(ng_solvers STATIC
    common/deadline.c
    registry.c
    s0_brute/brute.c
    solve.c)
# Headers shared inside the module (prefix ngi) are reachable as "solvers/common/...",
# "solvers/internal/..." and "core/internal/...".
target_include_directories(ng_solvers PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(ng_solvers
    PUBLIC ng_core
    PRIVATE ng_project_options)
