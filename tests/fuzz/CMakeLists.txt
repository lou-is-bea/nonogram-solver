# libFuzzer targets (preset "fuzz", Clang only). Run through scripts/check.sh (G9).
if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "NG_BUILD_FUZZ requires Clang (libFuzzer)")
endif()

add_executable(fuzz_non_parser fuzz_non_parser.c)
target_link_libraries(fuzz_non_parser PRIVATE ng_io ng_project_options)
target_link_options(fuzz_non_parser PRIVATE -fsanitize=fuzzer)
