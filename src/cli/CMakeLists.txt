# The commands as a library, so that unit tests can drive them (tests/unit/test_cli_solve.c).
add_library(nonogram_cli STATIC
    cli_args.c
    cli_load.c
    cli_usage.c
    cmd_list_solvers.c
    cmd_show.c
    cmd_solve.c
    cmd_verify.c)
target_include_directories(nonogram_cli PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(nonogram_cli
    PUBLIC
        ng_io
        ng_render
        ng_verify
        ng_solvers
    PRIVATE ng_project_options)

add_executable(nonogram main.c)
target_link_libraries(nonogram PRIVATE nonogram_cli ng_project_options)
