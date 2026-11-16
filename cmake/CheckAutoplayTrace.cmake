# Runs neo_wolf_autoplay twice with --trace and checks the event log (the autoplay.trace CTest
# test, docs/TESTING.md "Autoplay bot"): both runs pass, every event line has the documented
# "L<level> t=<seconds> f=<frame> <event>" prefix, the expected events are there, and the two
# traces are byte-identical (determinism).
# Usage: cmake -DGAME_EXE=<path> -DGAME_ARGS=<a;b;c> -DTRACE_DIR=<scratch folder>
#              -DEXPECT_EVENTS=<regex;regex;...> -P CheckAutoplayTrace.cmake
# Each EXPECT_EVENTS regex must match at least one line of the trace.

if(NOT GAME_EXE OR NOT TRACE_DIR)
    message(FATAL_ERROR "GAME_EXE and TRACE_DIR must be set")
endif()

file(REMOVE_RECURSE "${TRACE_DIR}")
file(MAKE_DIRECTORY "${TRACE_DIR}")
foreach(run 1 2)
    execute_process(
        COMMAND "${GAME_EXE}" ${GAME_ARGS} --pref-dir "${TRACE_DIR}/pref-${run}" --trace "${TRACE_DIR}/trace-${run}.txt"
        OUTPUT_VARIABLE out
        ERROR_VARIABLE out
        RESULT_VARIABLE result
        TIMEOUT 120)
    message("${out}")
    if(NOT "${result}" STREQUAL "0")
        message(FATAL_ERROR "Run ${run}: expected exit code 0, got '${result}'.")
    endif()
    if(NOT out MATCHES "Trace: ")
        message(FATAL_ERROR "Run ${run}: the output does not name the trace file.")
    endif()
endforeach()

file(STRINGS "${TRACE_DIR}/trace-1.txt" lines)
list(LENGTH lines line_count)
if(line_count LESS 3)
    message(FATAL_ERROR "The trace has only ${line_count} line(s).")
endif()
foreach(line IN LISTS lines)
    if(NOT line MATCHES "^#" AND NOT line MATCHES "^L[0-9]+ t=[0-9]+\\.[0-9][0-9][0-9] f=[0-9]+ [a-z-]+( |$)")
        message(FATAL_ERROR "Malformed trace line: '${line}'")
    endif()
endforeach()
foreach(regex IN LISTS EXPECT_EVENTS)
    set(found FALSE)
    foreach(line IN LISTS lines)
        if(line MATCHES "${regex}")
            set(found TRUE)
            break()
        endif()
    endforeach()
    if(NOT found)
        message(FATAL_ERROR "The trace has no line matching: ${regex}")
    endif()
endforeach()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E compare_files "${TRACE_DIR}/trace-1.txt" "${TRACE_DIR}/trace-2.txt"
    RESULT_VARIABLE different)
if(NOT different EQUAL 0)
    message(FATAL_ERROR "The traces of two identical runs differ: ${TRACE_DIR}/trace-1.txt, ${TRACE_DIR}/trace-2.txt")
endif()
message(STATUS "Trace OK: ${line_count} lines, identical in two runs.")
