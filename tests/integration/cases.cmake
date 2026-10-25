# CLI integration cases (TESTING.md §4.5): arguments, expected exit code, and reference
# outputs tests/data/expected/<case>.out (stdout) and <case>.err (stderr), reviewed by a human.
# Paths are relative to the repository root, the working directory of every case.

# ng_cli_case(<case> EXIT <code> ARGS <arguments...>)
function(ng_cli_case name)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "" "EXIT" "ARGS")
    list(JOIN ARG_ARGS "|" joined)
    add_test(NAME cli_${name}
        COMMAND ${CMAKE_COMMAND}
            -DNONOGRAM=$<TARGET_FILE:nonogram>
            -DCASE=${name}
            -DARGS=${joined}
            -DEXIT=${ARG_EXIT}
            -DEXPECTED_DIR=${PROJECT_SOURCE_DIR}/tests/data/expected
            -P ${CMAKE_CURRENT_LIST_DIR}/run_case.cmake
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
    set_tests_properties(cli_${name} PROPERTIES
        LABELS integration
        FAIL_REGULAR_EXPRESSION "${NG_TEST_FAILURE_REGEX}")
endfunction()

set(P tests/data/puzzles)
set(S tests/data/solutions)

# --- show: rendering (exit 0)
ng_cli_case(show_letter_p EXIT 0 ARGS show ${P}/tiny/letter_p.non)
ng_cli_case(show_letter_p_solution EXIT 0
    ARGS show ${P}/tiny/letter_p.non --solution ${S}/letter_p.sol)
ng_cli_case(show_letter_p_unicode EXIT 0
    ARGS show ${P}/tiny/letter_p.non --solution=${S}/letter_p.sol --unicode)
ng_cli_case(show_duck_solution EXIT 0 ARGS show ${P}/small/duck.non --solution ${S}/duck.sol)
ng_cli_case(show_tree_unicode EXIT 0
    ARGS show ${P}/small/tree.non --unicode --solution ${S}/tree.sol)
ng_cli_case(show_blank_cell EXIT 0 ARGS show ${P}/tiny/blank.non --solution ${S}/blank.sol)
ng_cli_case(show_summary_above_max_render EXIT 0
    ARGS show ${P}/small/duck.non --solution ${S}/duck.sol --max-render 9)
ng_cli_case(show_at_max_render EXIT 0 ARGS show ${P}/tiny/plus.non --max-render=5)

# --- input errors (exit 3)
ng_cli_case(show_missing_file EXIT 3 ARGS show ${P}/tiny/no_such_puzzle.non)
ng_cli_case(show_parse_error EXIT 3 ARGS show ${P}/invalid/zero_width.non)
ng_cli_case(show_inconsistent_puzzle EXIT 3 ARGS show ${P}/invalid/row_too_long.non)
ng_cli_case(show_solution_parse_error EXIT 3
    ARGS show ${P}/tiny/letter_p.non --solution ${P}/tiny/letter_p.non)
ng_cli_case(show_solution_wrong_size EXIT 3
    ARGS show ${P}/tiny/letter_p.non --solution ${S}/duck.sol)
ng_cli_case(show_missing_solution_file EXIT 3
    ARGS show ${P}/tiny/letter_p.non --solution ${S}/no_such_grid.sol)

# --- usage errors (exit 2) and help
ng_cli_case(no_command EXIT 2 ARGS)
ng_cli_case(unknown_command EXIT 2 ARGS frobnicate)
ng_cli_case(help EXIT 0 ARGS --help)
ng_cli_case(show_without_puzzle EXIT 2 ARGS show)
ng_cli_case(show_unknown_option EXIT 2 ARGS show ${P}/tiny/dot.non --colour)
ng_cli_case(show_two_puzzles EXIT 2 ARGS show ${P}/tiny/dot.non ${P}/tiny/dot.non)
ng_cli_case(show_missing_option_value EXIT 2 ARGS show ${P}/tiny/dot.non --solution)
ng_cli_case(show_invalid_max_render EXIT 2 ARGS show ${P}/tiny/dot.non --max-render 0)
ng_cli_case(show_non_numeric_max_render EXIT 2 ARGS show ${P}/tiny/dot.non --max-render=big)
