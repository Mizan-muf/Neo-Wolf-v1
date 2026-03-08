cmake_minimum_required(VERSION 3.21)
project(NeoWolf LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(neo_wolf src/main.cpp)
target_sources(neo_wolf PRIVATE
    engine/core/Engine.cpp
)
target_include_directories(neo_wolf PRIVATE ${CMAKE_SOURCE_DIR})

# Prefer SDL2 config package; fall back to CMake's FindSDL2 module.
find_package(SDL2 CONFIG QUIET)
if(SDL2_FOUND)
    target_link_libraries(neo_wolf PRIVATE SDL2::SDL2 SDL2::SDL2main)
else()
    find_package(SDL2 REQUIRED)
    target_include_directories(neo_wolf PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(neo_wolf PRIVATE ${SDL2_LIBRARIES})
endif()

# Copy runtime DLLs (e.g., SDL2.dll from vcpkg dynamic builds) next to the exe.
if(MSVC)
    add_custom_command(TARGET neo_wolf POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:neo_wolf>
            $<TARGET_FILE_DIR:neo_wolf>
        COMMAND_EXPAND_LISTS
    )
endif()
