# Criterion is a system package found through pkg-config (ADR-0003). Its headers are SYSTEM
# includes so that our strict warnings apply to our test code but not to Criterion itself.
find_package(PkgConfig REQUIRED)
pkg_check_modules(CRITERION REQUIRED IMPORTED_TARGET criterion>=2.4)
set_target_properties(PkgConfig::CRITERION PROPERTIES SYSTEM ON)

add_library(ng_test_support STATIC
    support/ng_test_fixture.c
    support/test_allocator.c)
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

ng_add_unit_test(allocator LIBS ng_core)
ng_add_unit_test(status LIBS ng_core)
ng_add_unit_test(support_allocator LIBS ng_core)
ng_add_unit_test(types LIBS ng_core)
ng_add_unit_test(version LIBS ng_core)

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

# --- CLI smoke tests ----------------------------------------------------------------------
add_test(NAME cli_version COMMAND nonogram --version)
set_tests_properties(cli_version PROPERTIES
    LABELS integration
    FAIL_REGULAR_EXPRESSION "${NG_TEST_FAILURE_REGEX}"
    PASS_REGULAR_EXPRESSION "^nonogram ${PROJECT_VERSION}\n$")
# Exactly exit code 2 (F-41) and the help text. The output is echoed so that a sanitizer report
# still fails the test (WILL_FAIL would invert that failure too).
add_test(NAME cli_without_arguments_is_usage_error
    COMMAND sh -c [=[out=$("$1" 2>&1); code=$?; printf '%s\n' "$out"; [ "$code" -eq 2 ] && printf '%s\n' "$out" | grep -q '^usage: nonogram']=] _ $<TARGET_FILE:nonogram>)
set_tests_properties(cli_without_arguments_is_usage_error PROPERTIES
    LABELS integration
    FAIL_REGULAR_EXPRESSION "${NG_TEST_FAILURE_REGEX}")

# Output failure: stdout on /dev/full (Linux) gives exit code 3.
add_test(NAME cli_version_output_failure
    COMMAND sh -c [=["$1" --version >/dev/full; [ "$?" -eq 3 ]]=] _ $<TARGET_FILE:nonogram>)
set_tests_properties(cli_version_output_failure PROPERTIES
    LABELS integration
    FAIL_REGULAR_EXPRESSION "${NG_TEST_FAILURE_REGEX}")
