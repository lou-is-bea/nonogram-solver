add_library(ng_render STATIC
    render_text.c)
# Internal headers of core (prefix ngi) are reachable as "core/internal/...".
target_include_directories(ng_render PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(ng_render
    PUBLIC ng_core
    PRIVATE ng_project_options)
