# The public version header is generated so that the project() version is the single source
# of truth.
configure_file(
    ${PROJECT_SOURCE_DIR}/include/nonogram/version.h.in
    ${PROJECT_BINARY_DIR}/include/nonogram/version.h
    @ONLY)

add_library(ng_core STATIC
    allocator.c
    buffer.c
    output.c
    grid.c
    puzzle.c
    status.c
    stream.c
    version.c)
target_include_directories(ng_core PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_BINARY_DIR}/include)
# Internal headers (src/core/internal, prefix ngi) are reachable as "core/internal/...".
target_include_directories(ng_core PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(ng_core PRIVATE ng_project_options)
target_compile_definitions(ng_core PRIVATE NG_VERSION_STRING="${PROJECT_VERSION}")
