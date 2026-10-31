# Benchmark harness (TESTING.md §4.8): measures a solver through the public API only.
add_library(ng_bench STATIC
    bench.c
    clock.c)
target_include_directories(ng_bench PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
# clock_gettime(CLOCK_MONOTONIC) is POSIX: only clock.c sees it (bench/README.md).
set_source_files_properties(clock.c PROPERTIES COMPILE_DEFINITIONS _POSIX_C_SOURCE=199309L)
target_link_libraries(ng_bench
    PUBLIC
        ng_verify
        ng_solvers
    PRIVATE ng_project_options)
