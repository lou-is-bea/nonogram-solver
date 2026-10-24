# Interface target until the first source file lands; it then becomes a STATIC library that
# links ng_core PUBLIC and ng_project_options PRIVATE (ARCHITECTURE.md §3).
add_library(ng_io INTERFACE)
target_link_libraries(ng_io INTERFACE ng_core)
