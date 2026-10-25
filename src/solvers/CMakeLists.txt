# Interface target until the registry lands (M4); it then becomes a STATIC library that links
# ng_core PUBLIC and ng_project_options PRIVATE. Never link ng_verify here (ADR-0007).
add_library(ng_solvers INTERFACE)
target_link_libraries(ng_solvers INTERFACE ng_core)
