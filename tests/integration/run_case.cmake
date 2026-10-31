# Runs one CLI case and compares its exit code, stdout and stderr with the reference files.
# Invoked by CTest as: cmake -DNONOGRAM=... -DCASE=... -DARGS=a|b|c -DEXIT=n
#                            -DEXPECTED_DIR=... -P run_case.cmake
# With NG_UPDATE_GOLDEN=1 in the environment, the reference files are rewritten instead
# (scripts/update_golden.sh); every change must then be reviewed in the diff.
string(REPLACE "|" ";" args "${ARGS}")
# NG_CLI_WRAPPER (for example a Valgrind command line, see scripts/check.sh G7) runs the CLI
# under a tool; the tool must not write to stdout or stderr, which are compared below.
set(wrapper "")
if(DEFINED ENV{NG_CLI_WRAPPER})
    separate_arguments(wrapper UNIX_COMMAND "$ENV{NG_CLI_WRAPPER}")
endif()
execute_process(
    COMMAND ${wrapper} ${NONOGRAM} ${args}
    RESULT_VARIABLE exit_code
    OUTPUT_VARIABLE actual_stdout
    ERROR_VARIABLE actual_stderr)

# Elapsed times change from run to run: they are replaced by "<ns>" before comparison (and in
# the reference files), in both the text (--stats) and the JSON outputs of nonogram solve.
string(REGEX REPLACE "\"elapsed_ns\": [0-9]+" "\"elapsed_ns\": <ns>" actual_stdout
    "${actual_stdout}")
string(REGEX REPLACE "elapsed: [0-9]+ ns" "elapsed: <ns> ns" actual_stdout "${actual_stdout}")
# Same for the times measured by nonogram bench.
string(REGEX REPLACE "\"(median|min|max)_ns\": [0-9]+" "\"\\1_ns\": <ns>" actual_stdout
    "${actual_stdout}")
string(REGEX REPLACE "median [0-9]+ ns \\(min [0-9]+, max [0-9]+\\)"
    "median <ns> ns (min <ns>, max <ns>)" actual_stdout "${actual_stdout}")

set(stdout_file ${EXPECTED_DIR}/${CASE}.out)
set(stderr_file ${EXPECTED_DIR}/${CASE}.err)

if(DEFINED ENV{NG_UPDATE_GOLDEN})
    file(WRITE ${stdout_file} "${actual_stdout}")
    file(WRITE ${stderr_file} "${actual_stderr}")
endif()

if(NOT exit_code STREQUAL EXIT)
    message(FATAL_ERROR "${CASE}: exit code ${exit_code}, expected ${EXIT}\n"
        "stdout:\n${actual_stdout}\nstderr:\n${actual_stderr}")
endif()
foreach(stream stdout stderr)
    file(READ ${${stream}_file} expected)
    if(NOT actual_${stream} STREQUAL expected)
        message(FATAL_ERROR "${CASE}: ${stream} differs from ${${stream}_file}\n"
            "--- expected\n${expected}--- actual\n${actual_${stream}}")
    endif()
endforeach()
