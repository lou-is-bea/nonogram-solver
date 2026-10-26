add_library(ng_io STATIC
    char_source.c
    non_metadata.c
    non_directives.c
    non_lexer.c
    non_parser.c
    non_writer.c
    sol_format.c
    text.c)
# Internal headers of core and io (prefix ngi) are reachable as "core/internal/..." and
# "io/internal/...".
target_include_directories(ng_io PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(ng_io
    PUBLIC ng_core
    PRIVATE ng_project_options)
