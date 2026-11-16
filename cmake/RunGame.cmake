# Runs the game executable once and checks its exit code and output (used by the
# game_smoke_* CTest tests).
# Usage: cmake -DGAME_EXE=<path> -DGAME_ARGS=<a;b;c> -DEXPECT_EXIT=<code>
#              [-DEXPECT_REGEX=<regex matched against stdout+stderr>] -P RunGame.cmake

if(NOT GAME_EXE OR NOT DEFINED EXPECT_EXIT)
    message(FATAL_ERROR "GAME_EXE and EXPECT_EXIT must be set")
endif()

execute_process(
    COMMAND "${GAME_EXE}" ${GAME_ARGS}
    OUTPUT_VARIABLE out
    ERROR_VARIABLE out
    RESULT_VARIABLE result
    TIMEOUT 90)

message("${out}")
if(NOT "${result}" STREQUAL "${EXPECT_EXIT}")
    message(FATAL_ERROR "Expected exit code ${EXPECT_EXIT}, got '${result}'.")
endif()
if(DEFINED EXPECT_REGEX AND NOT out MATCHES "${EXPECT_REGEX}")
    message(FATAL_ERROR "The output does not match the regular expression: ${EXPECT_REGEX}")
endif()
message(STATUS "Exit code ${result} as expected.")
