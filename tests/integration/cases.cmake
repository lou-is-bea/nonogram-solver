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

ng_cli_case(show_summary_only_too_wide EXIT 0 ARGS show ${P}/tiny/wide.non --max-render 5)
ng_cli_case(show_summary_only_too_tall EXIT 0 ARGS show ${P}/tiny/column.non --max-render 4)
ng_cli_case(show_largest_max_render EXIT 0 ARGS show ${P}/tiny/dot.non --max-render 2147483647)

# --- input errors (exit 3)
ng_cli_case(show_missing_file EXIT 3 ARGS show ${P}/tiny/no_such_puzzle.non)
ng_cli_case(show_parse_error EXIT 3 ARGS show ${P}/invalid/zero_width.non)
ng_cli_case(show_inconsistent_puzzle EXIT 3 ARGS show ${P}/invalid/row_too_long.non)
ng_cli_case(show_solution_parse_error EXIT 3
    ARGS show ${P}/tiny/letter_p.non --solution ${P}/tiny/letter_p.non)
ng_cli_case(show_solution_wrong_size EXIT 3
    ARGS show ${P}/tiny/letter_p.non --solution ${S}/duck.sol)
set(G tests/data/grids)
ng_cli_case(show_solution_wrong_height EXIT 3
    ARGS show ${P}/tiny/letter_p.non --solution ${G}/letter_p_three_rows.sol)
ng_cli_case(show_solution_wrong_width EXIT 3
    ARGS show ${P}/tiny/letter_p.non --solution ${G}/letter_p_three_columns.sol)
ng_cli_case(show_missing_solution_file EXIT 3
    ARGS show ${P}/tiny/letter_p.non --solution ${S}/no_such_grid.sol)

# --- verify (exit 0 valid, 1 incomplete or invalid, 3 input error, 2 usage)
set(G tests/data/grids)
ng_cli_case(verify_valid EXIT 0 ARGS verify ${P}/small/duck.non ${S}/duck.sol)
ng_cli_case(verify_valid_other_solution EXIT 0
    ARGS verify ${P}/multi/diagonal_n2.non ${G}/diagonal_other_solution.sol)
ng_cli_case(verify_bad_row EXIT 1 ARGS verify ${P}/tiny/letter_p.non ${G}/letter_p_bad_row.sol)
ng_cli_case(verify_bad_column EXIT 1
    ARGS verify ${P}/tiny/letter_p.non ${G}/letter_p_bad_column.sol)
ng_cli_case(verify_incomplete EXIT 1
    ARGS verify ${P}/tiny/letter_p.non ${G}/letter_p_incomplete.sol)
ng_cli_case(verify_filled_cell_on_empty_clue EXIT 1
    ARGS verify ${P}/tiny/blank.non ${G}/blank_filled.sol)
ng_cli_case(verify_wrong_height EXIT 3
    ARGS verify ${P}/tiny/letter_p.non ${G}/letter_p_three_rows.sol)
ng_cli_case(verify_missing_runs EXIT 1
    ARGS verify ${P}/tiny/letter_p.non ${G}/letter_p_empty_row.sol)
ng_cli_case(verify_extra_runs EXIT 1
    ARGS verify ${P}/tiny/letter_p.non ${G}/letter_p_extra_runs.sol)
ng_cli_case(verify_one_unknown_cell EXIT 1 ARGS verify ${P}/tiny/dot.non ${G}/dot_unknown.sol)
ng_cli_case(verify_option_second EXIT 2 ARGS verify ${P}/tiny/letter_p.non --fast)
ng_cli_case(verify_wrong_size EXIT 3 ARGS verify ${P}/tiny/letter_p.non ${S}/duck.sol)
ng_cli_case(verify_missing_grid EXIT 3 ARGS verify ${P}/tiny/letter_p.non ${G}/no_such.sol)
ng_cli_case(verify_invalid_puzzle EXIT 3 ARGS verify ${P}/invalid/bad_clue.non ${S}/dot.sol)
ng_cli_case(verify_one_argument EXIT 2 ARGS verify ${P}/tiny/letter_p.non)
ng_cli_case(verify_option EXIT 2 ARGS verify --fast ${P}/tiny/letter_p.non)

# --- usage errors (exit 2) and help
ng_cli_case(no_command EXIT 2 ARGS)
ng_cli_case(unknown_command EXIT 2 ARGS frobnicate)
ng_cli_case(help EXIT 0 ARGS --help)
ng_cli_case(show_without_puzzle EXIT 2 ARGS show)
ng_cli_case(show_unknown_option EXIT 2 ARGS show ${P}/tiny/dot.non --colour)
ng_cli_case(show_two_puzzles EXIT 2 ARGS show ${P}/tiny/dot.non ${P}/tiny/dot.non)
ng_cli_case(show_missing_option_value EXIT 2 ARGS show ${P}/tiny/dot.non --solution)
ng_cli_case(show_invalid_max_render EXIT 2 ARGS show ${P}/tiny/dot.non --max-render 0)
ng_cli_case(show_max_render_above_dim_max EXIT 2
    ARGS show ${P}/tiny/dot.non --max-render 2147483648)
ng_cli_case(show_max_render_twenty_digits EXIT 2
    ARGS show ${P}/tiny/dot.non --max-render=99999999999999999999)
ng_cli_case(show_option_with_suffix EXIT 2 ARGS show ${P}/tiny/dot.non --solutionfoo)
ng_cli_case(show_repeated_option EXIT 2 ARGS show ${P}/tiny/dot.non --unicode --unicode)
ng_cli_case(show_repeated_max_render EXIT 2
    ARGS show ${P}/tiny/dot.non --max-render 3 --max-render=4)
ng_cli_case(show_repeated_solution EXIT 2
    ARGS show ${P}/tiny/dot.non --solution ${S}/dot.sol --solution=${S}/dot.sol)
ng_cli_case(show_option_instead_of_value EXIT 2
    ARGS show ${P}/tiny/dot.non --solution --unicode)
ng_cli_case(show_non_numeric_max_render EXIT 2 ARGS show ${P}/tiny/dot.non --max-render=big)

# --- solve and list-solvers (M4). Puzzles of at most 16 cells: every case also runs under
# Valgrind (G7), where S0 examines about 3 x 10^6 candidates per second.
set(M ${P}/multi)
ng_cli_case(solve_solved EXIT 0 ARGS solve ${P}/tiny/l_shape.non)
ng_cli_case(solve_named_solver EXIT 0 ARGS solve ${P}/tiny/dot.non --solver brute)
ng_cli_case(solve_stats EXIT 0 ARGS solve ${P}/tiny/l_shape.non --stats)
ng_cli_case(solve_json EXIT 0 ARGS solve ${P}/tiny/l_shape.non --format=json)
ng_cli_case(solve_json_unsolvable EXIT 1
    ARGS solve ${P}/unsolvable/crossed.non --format json --stats)
ng_cli_case(solve_text_format EXIT 0 ARGS solve ${P}/tiny/dot.non --format text)
ng_cli_case(solve_unsolvable EXIT 1 ARGS solve ${P}/unsolvable/crossed.non)
ng_cli_case(solve_count_exact EXIT 0 ARGS solve ${M}/checker_n2.non --count 5)
ng_cli_case(solve_count_at_least EXIT 0 ARGS solve ${M}/permutation_n24.non --count=3)
ng_cli_case(solve_count_unsolvable EXIT 1 ARGS solve ${P}/unsolvable/crossed.non --count 2)
ng_cli_case(solve_limit_reached EXIT 1 ARGS solve ${M}/permutation_n24.non --max-nodes 10)
ng_cli_case(solve_limit_keeps_solution EXIT 1
    ARGS solve ${M}/diagonal_n2.non --count 3 --max-nodes=8 --format json)
ng_cli_case(solve_too_large EXIT 2 ARGS solve ${P}/small/duck.non)
ng_cli_case(solve_memory_too_small EXIT 2 ARGS solve ${P}/tiny/dot.non --max-memory 1)
ng_cli_case(solve_large_limits EXIT 0
    ARGS solve ${P}/tiny/dot.non --max-memory 18446744073709551615 --max-time 60000)
ng_cli_case(solve_unknown_solver EXIT 2 ARGS solve ${P}/tiny/dot.non --solver nope)
ng_cli_case(solve_missing_puzzle EXIT 2 ARGS solve --stats)
ng_cli_case(solve_two_puzzles EXIT 2 ARGS solve ${P}/tiny/dot.non ${P}/tiny/dot.non)
ng_cli_case(solve_unknown_option EXIT 2 ARGS solve ${P}/tiny/dot.non --fast)
ng_cli_case(solve_missing_value EXIT 2 ARGS solve ${P}/tiny/dot.non --solver)
ng_cli_case(solve_option_instead_of_value EXIT 2 ARGS solve ${P}/tiny/dot.non --count --stats)
ng_cli_case(solve_zero_count EXIT 2 ARGS solve ${P}/tiny/dot.non --count 0)
ng_cli_case(solve_count_above_limit EXIT 2 ARGS solve ${P}/tiny/dot.non --count 4294967296)
ng_cli_case(solve_non_numeric_max_nodes EXIT 2 ARGS solve ${P}/tiny/dot.non --max-nodes=ten)
ng_cli_case(solve_zero_max_time EXIT 2 ARGS solve ${P}/tiny/dot.non --max-time 0)
ng_cli_case(solve_invalid_format EXIT 2 ARGS solve ${P}/tiny/dot.non --format xml)
ng_cli_case(solve_repeated_stats EXIT 2 ARGS solve ${P}/tiny/dot.non --stats --stats)
ng_cli_case(solve_repeated_count EXIT 2 ARGS solve ${P}/tiny/dot.non --count 2 --count=3)
ng_cli_case(solve_missing_file EXIT 3 ARGS solve ${P}/tiny/no_such.non)
ng_cli_case(solve_invalid_puzzle EXIT 3 ARGS solve ${P}/invalid/bad_clue.non)
ng_cli_case(solve_unwritable_output EXIT 3
    ARGS solve ${P}/tiny/dot.non --output tests/data/no_such_dir/dot.sol)
ng_cli_case(list_solvers EXIT 0 ARGS list-solvers)
ng_cli_case(list_solvers_argument EXIT 2 ARGS list-solvers brute)
ng_cli_case(solve_limit_count_text EXIT 1
    ARGS solve ${M}/diagonal_n2.non --count 3 --max-nodes 8)
