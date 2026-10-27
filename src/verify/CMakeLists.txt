# The oracle (ADR-0007): depends on core only, never on ng_solvers.
add_library(ng_verify STATIC
    verify.c)
# Internal headers of core (prefix ngi) are reachable as "core/internal/...".
target_include_directories(ng_verify PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(ng_verify
    PUBLIC ng_core
    PRIVATE ng_project_options)
