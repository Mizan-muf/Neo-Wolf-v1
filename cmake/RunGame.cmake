# Runs the game executable once and checks its exit code and output (used by the
# game_smoke_* CTest tests), and optionally the size of the screenshot it saves.
# Usage: cmake -DGAME_EXE=<path> -DGAME_ARGS=<a;b;c> -DEXPECT_EXIT=<code>
#              [-DEXPECT_REGEX=<regex matched against stdout+stderr>]
#              [-DEXPECT_BMP=<screenshot path> -DEXPECT_BMP_SIZE=<width>x<height>] -P RunGame.cmake

if(NOT GAME_EXE OR NOT DEFINED EXPECT_EXIT)
    message(FATAL_ERROR "GAME_EXE and EXPECT_EXIT must be set")
endif()

if(DEFINED EXPECT_BMP)
    file(REMOVE "${EXPECT_BMP}")  # a screenshot left by an earlier run must not pass
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
if(DEFINED EXPECT_BMP)
    if(NOT EXISTS "${EXPECT_BMP}")
        message(FATAL_ERROR "No screenshot was saved at ${EXPECT_BMP}.")
    endif()
    # BMP header: the width and height are little-endian 32-bit numbers at bytes 18 and 22.
    file(READ "${EXPECT_BMP}" header LIMIT 26 HEX)
    set(bmp_size "")
    foreach(offset 36 44)
        set(number "")
        foreach(byte 0 1 2 3)
            math(EXPR at "${offset} + ${byte} * 2")
            string(SUBSTRING "${header}" ${at} 2 digits)
            set(number "${digits}${number}")
        endforeach()
        math(EXPR value "0x${number}")
        list(APPEND bmp_size ${value})
    endforeach()
    list(JOIN bmp_size "x" bmp_size)
    if(NOT bmp_size STREQUAL EXPECT_BMP_SIZE)
        message(FATAL_ERROR "The screenshot is ${bmp_size}, expected ${EXPECT_BMP_SIZE}.")
    endif()
    message(STATUS "Screenshot size ${bmp_size} as expected.")
endif()
message(STATUS "Exit code ${result} as expected.")
