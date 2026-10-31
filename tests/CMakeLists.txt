# Criterion is a system package found through pkg-config (ADR-0003). Its headers are SYSTEM
# includes so that our strict warnings apply to our test code but not to Criterion itself.
find_package(PkgConfig REQUIRED)
pkg_check_modules(CRITERION REQUIRED IMPORTED_TARGET criterion>=2.4)
set_target_properties(PkgConfig::CRITERION PROPERTIES SYSTEM ON)

add_library(ng_test_support STATIC
    support/ng_test_fixture.c
    support/test_allocator.c
    support/test_puzzles.c
    support/test_rng.c
    support/test_streams.c)
target_include_directories(ng_test_support PUBLIC support)
target_link_libraries(ng_test_support
    PUBLIC PkgConfig::CRITERION ng_core
    PRIVATE ng_project_options)

# Criterion 2.4 records a test as passed before LeakSanitizer reports at process exit, and
# ignores assertions raised in .fini. Both kinds of failure are therefore detected in the
# output: a sanitizer report or the common fixture's NG_FIXTURE_FAILURE marker fails the test.
set(NG_TEST_FAILURE_REGEX "ERROR: (Address|Leak)Sanitizer;runtime error:;NG_FIXTURE_FAILURE")

# ng_add_unit_test(<module> LIBS <targets...>)
# Builds tests/unit/test_<module>.c into one Criterion executable registered with label "unit".
function(ng_add_unit_test module)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "" "" "LIBS")
    set(target test_${module})
    add_executable(${target} unit/${target}.c)
    target_link_libraries(${target} PRIVATE ng_test_support ${ARG_LIBS} ng_project_options)
    set_target_properties(${target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/unit)
    add_test(NAME ${target} COMMAND ${target})
    set_tests_properties(${target} PROPERTIES
        LABELS unit
        FAIL_REGULAR_EXPRESSION "${NG_TEST_FAILURE_REGEX}")
endfunction()

# ng_add_property_test(<name> LIBS <targets...>)
# Builds tests/property/test_prop_<name>.c into a Criterion executable labelled "property".
# Seeds come from NG_TEST_SEED when set (TESTING.md §4.4).
function(ng_add_property_test name)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "" "" "LIBS")
    set(target test_prop_${name})
    add_executable(${target} property/${target}.c)
    target_link_libraries(${target} PRIVATE ng_test_support ${ARG_LIBS} ng_project_options)
    set_target_properties(${target} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/property)
    add_test(NAME ${target} COMMAND ${target})
    set_tests_properties(${target} PROPERTIES
        LABELS property
        FAIL_REGULAR_EXPRESSION "${NG_TEST_FAILURE_REGEX}")
endfunction()

# ng_add_massive_test(<name> LIBS <targets...>)
# Builds tests/massive/test_<name>.c, labelled "massive": excluded from the everyday presets,
# run with `ctest --preset massive` (TESTING.md §4.6, G12).
function(ng_add_massive_test name)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "" "" "LIBS")
    set(target test_${name})
    add_executable(${target} massive/${target}.c)
    target_link_libraries(${target} PRIVATE ng_test_support ${ARG_LIBS} ng_project_options)
    set_target_properties(${target} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/massive)
    add_test(NAME ${target} COMMAND ${target})
    set_tests_properties(${target} PROPERTIES
        LABELS massive
        FAIL_REGULAR_EXPRESSION "${NG_TEST_FAILURE_REGEX}")
endfunction()

ng_add_massive_test(massive LIBS ng_io ng_verify)

ng_add_property_test(grid LIBS ng_core)
ng_add_property_test(io LIBS ng_io)
ng_add_property_test(solver_brute LIBS ng_solvers ng_verify)
ng_add_property_test(verify LIBS ng_verify)

ng_add_unit_test(allocator LIBS ng_core)
ng_add_unit_test(grid LIBS ng_core)
ng_add_unit_test(io_non LIBS ng_io)
ng_add_unit_test(io_sol LIBS ng_io)
ng_add_unit_test(puzzle LIBS ng_core)
ng_add_unit_test(render LIBS ng_render)
ng_add_unit_test(solver LIBS ng_solvers)
ng_add_unit_test(solver_brute LIBS ng_solvers ng_verify)
# The vtable of S0 is internal to the solvers module (PLAYBOOKS P1).
target_include_directories(test_solver_brute PRIVATE ${PROJECT_SOURCE_DIR}/src)
ng_add_unit_test(solver_internal LIBS ng_solvers)
# Deadline and registry lookups are internal to the solvers module.
target_include_directories(test_solver_internal PRIVATE ${PROJECT_SOURCE_DIR}/src)
ng_add_unit_test(status LIBS ng_core)
ng_add_unit_test(stream LIBS ng_core)
ng_add_unit_test(support_allocator LIBS ng_core)
ng_add_unit_test(support_rng LIBS ng_core)
ng_add_unit_test(support_streams LIBS ng_core)
ng_add_unit_test(types LIBS ng_core)
ng_add_unit_test(verify LIBS ng_verify)
ng_add_unit_test(version LIBS ng_core)

# --- Corpus (tests/data, see tests/data/README.md) -------------------------------------------
# The file lists are generated so that adding a corpus file needs no code change.
set(ng_corpus_dir ${PROJECT_SOURCE_DIR}/tests/data)
file(GLOB ng_corpus_solved_files CONFIGURE_DEPENDS
    ${ng_corpus_dir}/puzzles/tiny/*.non ${ng_corpus_dir}/puzzles/small/*.non)
file(GLOB ng_corpus_other_files CONFIGURE_DEPENDS
    ${ng_corpus_dir}/puzzles/multi/*.non ${ng_corpus_dir}/puzzles/unsolvable/*.non
    ${ng_corpus_dir}/puzzles/large/*.non)
file(GLOB ng_corpus_invalid_files CONFIGURE_DEPENDS ${ng_corpus_dir}/puzzles/invalid/*.non)
set(ng_corpus_header "/* Generated by tests/CMakeLists.txt: do not edit. */\n")
string(APPEND ng_corpus_header "#ifndef NG_TESTS_CORPUS_FILES_H\n#define NG_TESTS_CORPUS_FILES_H\n\n")
string(APPEND ng_corpus_header "typedef struct ng_corpus_solved_entry {\n    const char *puzzle;\n    const char *solution;\n} ng_corpus_solved_entry;\n\n")
string(APPEND ng_corpus_header "typedef struct ng_corpus_invalid_entry {\n    const char *puzzle;\n    const char *expected;\n} ng_corpus_invalid_entry;\n\n")
string(APPEND ng_corpus_header "[[maybe_unused]] static ng_corpus_solved_entry ng_corpus_solved[] = {\n")
foreach(file IN LISTS ng_corpus_solved_files)
    get_filename_component(name ${file} NAME_WE)
    string(APPEND ng_corpus_header "    {\"${file}\", \"${ng_corpus_dir}/solutions/${name}.sol\"},\n")
endforeach()

string(APPEND ng_corpus_header "};\n\n[[maybe_unused]] static const char *ng_corpus_other[] = {\n")
foreach(file IN LISTS ng_corpus_other_files)
    string(APPEND ng_corpus_header "    \"${file}\",\n")
endforeach()
string(APPEND ng_corpus_header "};\n\n[[maybe_unused]] static ng_corpus_invalid_entry ng_corpus_invalid[] = {\n")
foreach(file IN LISTS ng_corpus_invalid_files)
    get_filename_component(dir ${file} DIRECTORY)
    get_filename_component(name ${file} NAME_WE)
    string(APPEND ng_corpus_header "    {\"${file}\", \"${dir}/${name}.expected\"},\n")
endforeach()
string(APPEND ng_corpus_header "};\n\n#endif /* NG_TESTS_CORPUS_FILES_H */\n")
file(CONFIGURE OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/generated/corpus_files.h
    CONTENT "${ng_corpus_header}")

ng_add_unit_test(cli_solve LIBS nonogram_cli)
# The test reads a corpus puzzle and writes a scratch file: absolute paths.
target_compile_definitions(test_cli_solve PRIVATE
    NG_SOURCE_DIR="${PROJECT_SOURCE_DIR}" NG_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}")
ng_add_unit_test(corpus LIBS ng_io)
target_include_directories(test_corpus PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated)

# --- Exhaustive tests (docs/solvers/s0_brute.md §6) --------------------------------------------
# Full enumerations of the corpus, up to 2^25 candidates per puzzle: label "exhaustive", built
# in build/<preset>/tests/exhaustive/, which keeps them out of Valgrind (G7 runs the binaries of
# unit/ and property/ only). G3 runs them in dev and release; G6 and G8 run every label but
# "massive".
function(ng_add_exhaustive_test name)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "" "" "LIBS")
    set(target test_exhaustive_${name})
    add_executable(${target} exhaustive/${target}.c)
    target_link_libraries(${target} PRIVATE ng_test_support ${ARG_LIBS} ng_project_options)
    target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated)
    set_target_properties(${target} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/exhaustive)
    add_test(NAME ${target} COMMAND ${target})
    set_tests_properties(${target} PROPERTIES
        LABELS exhaustive
        FAIL_REGULAR_EXPRESSION "${NG_TEST_FAILURE_REGEX}")
endfunction()

ng_add_exhaustive_test(brute LIBS ng_solvers ng_verify ng_io)

# --- Public header self-containment (CODING_STANDARDS.md §3) ------------------------------
# Each public header is compiled alone in its own translation unit; a header that misses an
# include breaks the build.
file(GLOB ng_public_headers CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/include/nonogram/*.h)
file(GLOB ng_public_header_templates CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/include/nonogram/*.h.in)
foreach(template IN LISTS ng_public_header_templates)
    get_filename_component(name ${template} NAME_WLE)
    list(APPEND ng_public_headers ${PROJECT_BINARY_DIR}/include/nonogram/${name})
endforeach()
set(ng_header_check_sources)
foreach(header IN LISTS ng_public_headers)
    get_filename_component(name ${header} NAME_WE)
    set(source ${CMAKE_CURRENT_BINARY_DIR}/header_check/${name}.c)
    file(CONFIGURE OUTPUT ${source} CONTENT "#include \"nonogram/${name}.h\"\n")
    list(APPEND ng_header_check_sources ${source})
endforeach()
add_library(ng_header_check OBJECT ${ng_header_check_sources})
target_include_directories(ng_header_check PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_BINARY_DIR}/include)
target_link_libraries(ng_header_check PRIVATE ng_project_options)

# --- CLI integration tests ------------------------------------------------------------------
add_test(NAME cli_version
    COMMAND sh -c [=[$NG_CLI_WRAPPER "$1" --version]=] _ $<TARGET_FILE:nonogram>)
set_tests_properties(cli_version PROPERTIES
    LABELS integration
    FAIL_REGULAR_EXPRESSION "${NG_TEST_FAILURE_REGEX}"
    PASS_REGULAR_EXPRESSION "^nonogram ${PROJECT_VERSION}\n$")
include(integration/cases.cmake)

# Output failures: stdout on /dev/full (Linux) gives exit code 3. --version and --help write
# less than a buffer, so the error only shows when stdout is flushed. Like every CLI case, these
# tests run under NG_CLI_WRAPPER (Valgrind in G7).
foreach(option IN ITEMS version help)
    add_test(NAME cli_${option}_output_failure
        COMMAND sh -c [=[$NG_CLI_WRAPPER "$1" "--$2" >/dev/full; [ "$?" -eq 3 ]]=]
            _ $<TARGET_FILE:nonogram> ${option})
    set_tests_properties(cli_${option}_output_failure PROPERTIES
        LABELS integration
        FAIL_REGULAR_EXPRESSION "${NG_TEST_FAILURE_REGEX}")
endforeach()

# show must also print a message, both when the error only shows at the final flush (small
# output) and when the renderer itself gets it (output larger than the buffers: the 100 x 100
# puzzle renders about 25 KB).
foreach(output_case IN ITEMS "small:tests/data/puzzles/tiny/dot.non"
                             "large:tests/data/puzzles/large/stripes_100.non")
    string(REPLACE ":" ";" parts "${output_case}")
    list(GET parts 0 size)
    list(GET parts 1 puzzle)
    add_test(NAME cli_show_output_failure_${size}
        COMMAND sh -c [=[err=$($NG_CLI_WRAPPER "$1" show "$2" 2>&1 >/dev/full); code=$?; printf '%s\n' "$err"; [ "$code" -eq 3 ] && printf '%s\n' "$err" | grep -q 'cannot write the output']=] _ $<TARGET_FILE:nonogram> ${puzzle}
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
    set_tests_properties(cli_show_output_failure_${size} PROPERTIES
        LABELS integration
        FAIL_REGULAR_EXPRESSION "${NG_TEST_FAILURE_REGEX}")
endforeach()

# solve and list-solvers report their write failures the same way (code 3).
foreach(command IN ITEMS "solve|tests/data/puzzles/tiny/dot.non" "list-solvers|")
    string(REPLACE "|" ";" parts "${command}")
    list(GET parts 0 name)
    list(GET parts 1 input)
    string(REPLACE "-" "_" test_name "${name}")
    add_test(NAME cli_${test_name}_output_failure
        COMMAND sh -c [=[err=$($NG_CLI_WRAPPER "$1" $2 $3 2>&1 >/dev/full); code=$?; printf '%s\n' "$err"; [ "$code" -eq 3 ] && printf '%s\n' "$err" | grep -q 'cannot write the output']=] _ $<TARGET_FILE:nonogram> ${name} ${input}
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
    set_tests_properties(cli_${test_name}_output_failure PROPERTIES
        LABELS integration
        FAIL_REGULAR_EXPRESSION "${NG_TEST_FAILURE_REGEX}")
endforeach()

# solve --output writes the solution as a .sol file, byte for byte the reference one.
add_test(NAME cli_solve_output_file
    COMMAND sh -c [=[out="$4/cli_solve_output.sol"; rm -f "$out"; $NG_CLI_WRAPPER "$1" solve "$2" --output "$out" > /dev/null && cmp "$out" "$3"]=] _ $<TARGET_FILE:nonogram> tests/data/puzzles/tiny/l_shape.non tests/data/solutions/l_shape.sol ${CMAKE_CURRENT_BINARY_DIR}
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
set_tests_properties(cli_solve_output_file PROPERTIES
    LABELS integration
    FAIL_REGULAR_EXPRESSION "${NG_TEST_FAILURE_REGEX}")

# solve --output reports a write failure (code 3). The partial file is not removed: the path
# may be a device, such as /dev/full here.
add_test(NAME cli_solve_output_write_failure
    COMMAND sh -c [=[err=$($NG_CLI_WRAPPER "$1" solve "$2" --output /dev/full 2>&1 >/dev/null); code=$?; printf '%s\n' "$err"; [ "$code" -eq 3 ] && printf '%s\n' "$err" | grep -q "cannot write '/dev/full'"]=] _ $<TARGET_FILE:nonogram> tests/data/puzzles/tiny/l_shape.non
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
set_tests_properties(cli_solve_output_write_failure PROPERTIES
    LABELS integration
    FAIL_REGULAR_EXPRESSION "${NG_TEST_FAILURE_REGEX}")

# solve --output writes nothing when the puzzle is not solved.
add_test(NAME cli_solve_output_unsolved
    COMMAND sh -c [=[out="$3/cli_solve_output_unsolved.sol"; rm -f "$out"; $NG_CLI_WRAPPER "$1" solve "$2" --output "$out" > /dev/null; code=$?; [ "$code" -eq 1 ] && [ ! -e "$out" ]]=] _ $<TARGET_FILE:nonogram> tests/data/puzzles/unsolvable/crossed.non ${CMAKE_CURRENT_BINARY_DIR}
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
set_tests_properties(cli_solve_output_unsolved PROPERTIES
    LABELS integration
    FAIL_REGULAR_EXPRESSION "${NG_TEST_FAILURE_REGEX}")

# The JSON output escapes the puzzle path: a name with a quote, a backslash and a tab.
add_test(NAME cli_solve_json_escapes_the_path
    COMMAND sh -c [=[dir="$3/json escape"; mkdir -p "$dir"; in="$dir/q\"b\\$(printf '\t')t.non"; cp "$2" "$in"; $NG_CLI_WRAPPER "$1" solve "$in" --format json | grep -F -q 'json escape/q\"b\\\u0009t.non"']=] _ $<TARGET_FILE:nonogram> tests/data/puzzles/tiny/dot.non ${CMAKE_CURRENT_BINARY_DIR}
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
set_tests_properties(cli_solve_json_escapes_the_path PROPERTIES
    LABELS integration
    FAIL_REGULAR_EXPRESSION "${NG_TEST_FAILURE_REGEX}")

# verify reports its write failure the same way.
add_test(NAME cli_verify_output_failure
    COMMAND sh -c [=[err=$($NG_CLI_WRAPPER "$1" verify "$2" "$3" 2>&1 >/dev/full); code=$?; printf '%s\n' "$err"; [ "$code" -eq 3 ] && printf '%s\n' "$err" | grep -q 'cannot write the output']=] _ $<TARGET_FILE:nonogram> tests/data/puzzles/tiny/dot.non tests/data/solutions/dot.sol
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
set_tests_properties(cli_verify_output_failure PROPERTIES
    LABELS integration
    FAIL_REGULAR_EXPRESSION "${NG_TEST_FAILURE_REGEX}")
