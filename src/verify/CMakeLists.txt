# Interface target until the first source file lands; it then becomes a STATIC library that
# links ng_core PUBLIC and ng_project_options PRIVATE (ARCHITECTURE.md §3).
add_library(ng_verify INTERFACE)
target_link_libraries(ng_verify INTERFACE ng_core)
