add_executable(nonogram
    main.c)
target_link_libraries(nonogram PRIVATE
    ng_io
    ng_render
    ng_verify
    ng_solvers
    ng_project_options)
