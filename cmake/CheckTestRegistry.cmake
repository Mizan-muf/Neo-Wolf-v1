# Verifies that the native test cases registered with CTest (NEOWOLF_NATIVE_TESTS in
# CMakeLists.txt) match the test table compiled into neo_wolf_tests.
# Usage: cmake -DTEST_EXE=<path> -DEXPECTED=<a;b;c> -P CheckTestRegistry.cmake

if(NOT TEST_EXE OR NOT DEFINED EXPECTED)
    message(FATAL_ERROR "TEST_EXE and EXPECTED must be set")
endif()

execute_process(
    COMMAND "${TEST_EXE}" --list
    OUTPUT_VARIABLE list_output
    RESULT_VARIABLE list_result)
if(NOT list_result EQUAL 0)
    message(FATAL_ERROR "'${TEST_EXE} --list' failed with exit code ${list_result}")
endif()

string(REPLACE "\r" "" list_output "${list_output}")
string(REGEX REPLACE "\n$" "" list_output "${list_output}")
string(REPLACE "\n" ";" actual "${list_output}")

set(missing "")
foreach(name IN LISTS actual)
    if(NOT name IN_LIST EXPECTED)
        list(APPEND missing "${name}")
    endif()
endforeach()
set(stale "")
foreach(name IN LISTS EXPECTED)
    if(NOT name IN_LIST actual)
        list(APPEND stale "${name}")
    endif()
endforeach()

if(missing OR stale)
    message(FATAL_ERROR
        "CTest registry is out of sync with tests/LogicTests.cpp.\n"
        "  In the executable but not registered in CMakeLists.txt: ${missing}\n"
        "  Registered in CMakeLists.txt but not in the executable: ${stale}")
endif()

list(LENGTH actual count)
message(STATUS "All ${count} native test cases are registered with CTest.")
