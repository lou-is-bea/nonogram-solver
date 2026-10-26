cmake_minimum_required(VERSION 3.21)

project(nonogram
    VERSION 0.0.0
    DESCRIPTION "Nonogram solving engine"
    LANGUAGES C)

# --- Language: strict C23, no GNU extensions (ADR-0001) ------------------------------
set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# CMake 3.28 spells C23 "-std=c2x" for GCC; the standard flag of ADR-0001 is "-std=c23",
# accepted by every supported compiler (GCC >= 14, Clang >= 19).
set(CMAKE_C23_STANDARD_COMPILE_OPTION "-std=c23")
set(CMAKE_C23_EXTENSION_COMPILE_OPTION "-std=gnu23")

# CMAKE_C_STANDARD 23 may silently map to an incomplete -std=c2x on older compilers, so the
# C23 features the project relies on are probed explicitly.
include(CheckCSourceCompiles)
set(CMAKE_REQUIRED_FLAGS "-std=c23")
check_c_source_compiles([=[
    #include <stdbit.h>
    #include <stdckdint.h>
    #include <stdint.h>
    enum probe : uint8_t { PROBE_A = 0 };
    constexpr uint32_t probe_limit = 1'000;
    [[nodiscard]] static int probe(void) { return 0; }
    int main(void)
    {
        uint64_t r = 0;
        void *p = nullptr;
        bool overflow = ckd_mul(&r, (uint64_t)probe_limit, (uint64_t)2u);
        return (p == nullptr && !overflow && stdc_count_ones(r) != 0u) ? probe() : 1;
    }
]=] NG_HAS_C23)
unset(CMAKE_REQUIRED_FLAGS)
if(NOT NG_HAS_C23)
    message(FATAL_ERROR
        "${CMAKE_C_COMPILER} does not support the C23 features required by the project "
        "(-std=c23, <stdckdint.h>, <stdbit.h>, constexpr, nullptr). "
        "Use GCC >= 14 or Clang >= 19 (see CONTRIBUTING.md).")
endif()

# --- Options ---------------------------------------------------------------------------
option(NG_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)
option(NG_BUILD_TESTS "Build the test suite (requires Criterion)" ON)
option(NG_BUILD_FUZZ "Build libFuzzer targets (requires Clang)" OFF)

# --- Project-wide compile options (CODING_STANDARDS.md §1) -----------------------------
# Every project target links ng_project_options PRIVATE; nothing touches CMAKE_C_FLAGS.
add_library(ng_project_options INTERFACE)
target_compile_options(ng_project_options INTERFACE
    -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow
    -Wstrict-prototypes -Wmissing-prototypes -Wvla -Wcast-qual -Wformat=2
    -Wnull-dereference -Wdouble-promotion -Wundef -Wimplicit-fallthrough
    $<$<BOOL:${NG_WARNINGS_AS_ERRORS}>:-Werror>)

# --- Modules (ARCHITECTURE.md §3) ------------------------------------------------------
add_subdirectory(src/core)
add_subdirectory(src/io)
add_subdirectory(src/render)
add_subdirectory(src/verify)
add_subdirectory(src/solvers)
add_subdirectory(src/cli)

if(NG_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(NG_BUILD_FUZZ)
    add_subdirectory(tests/fuzz)
endif()
