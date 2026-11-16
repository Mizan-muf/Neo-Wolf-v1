# 3.25: FetchContent_Declare(... SYSTEM), and CMakePresets.json schema version 6.
cmake_minimum_required(VERSION 3.25)

# --- Settings that must be known before project() -------------------------------------
# macOS: std::filesystem needs 10.15. A universal build is selected with
# CMAKE_OSX_ARCHITECTURES="arm64;x86_64" (the `macos` presets do this).
if(NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS version")
endif()

# MSVC: link the C runtime statically everywhere (our targets and SDL), so the game
# needs no Visual C++ redistributable. Must be set before SDL is fetched. A value given on
# the command line (-DCMAKE_MSVC_RUNTIME_LIBRARY=...) wins.
if(NOT DEFINED CMAKE_MSVC_RUNTIME_LIBRARY)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

project(NeoWolf VERSION 0.9.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

get_property(NEOWOLF_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(NOT NEOWOLF_MULTI_CONFIG AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(NEOWOLF_SYSTEM_SDL2 "Use an installed SDL2 (find_package) instead of building the pinned source release" OFF)
option(NEOWOLF_WERROR "Treat compiler warnings as errors for Neo-Wolf targets (never for SDL)" OFF)
option(NEOWOLF_BUILD_TESTS "Build the native test executable and register CTest tests" ON)

# --- SDL2 ------------------------------------------------------------------------------
# Pinned SDL2 release (newest stable 2.x release at the time of pinning), downloaded as the
# official source tarball and checked against its SHA-256 (no Git needed).
# Offline builds: -DFETCHCONTENT_SOURCE_DIR_SDL2=/path/to/SDL2-2.32.10 (the extracted
# tarball or a checkout of tag release-2.32.10).
set(NEOWOLF_SDL2_VERSION "2.32.10")
set(NEOWOLF_SDL2_URL
    "https://github.com/libsdl-org/SDL/releases/download/release-${NEOWOLF_SDL2_VERSION}/SDL2-${NEOWOLF_SDL2_VERSION}.tar.gz")
set(NEOWOLF_SDL2_SHA256 "5f5993c530f084535c65a6879e9b26ad441169b3e25d789d83287040a9ca5165")

# neowolf_sdl2 hides which SDL2 flavour is used from the rest of the build.
add_library(neowolf_sdl2 INTERFACE)

if(NEOWOLF_SYSTEM_SDL2)
    find_package(SDL2 CONFIG REQUIRED)
    if(TARGET SDL2::SDL2)
        target_link_libraries(neowolf_sdl2 INTERFACE SDL2::SDL2)
    elseif(TARGET SDL2::SDL2-static)
        target_link_libraries(neowolf_sdl2 INTERFACE SDL2::SDL2-static)
    elseif(SDL2_INCLUDE_DIRS AND SDL2_LIBRARIES)
        target_include_directories(neowolf_sdl2 INTERFACE ${SDL2_INCLUDE_DIRS})
        target_link_libraries(neowolf_sdl2 INTERFACE ${SDL2_LIBRARIES})
    else()
        message(FATAL_ERROR "SDL2 was found but provides neither the SDL2::SDL2 target "
                            "nor SDL2_INCLUDE_DIRS/SDL2_LIBRARIES.")
    endif()
    message(STATUS "Neo-Wolf: using system SDL2 ${SDL2_VERSION} (${SDL2_DIR})")
else()
    include(FetchContent)

    # SDL's options. They must be cache variables: SDL's policy level predates CMP0077, so
    # its option() calls would ignore normal variables.
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST OFF CACHE BOOL "" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    set(SDL_INSTALL_TESTS OFF CACHE BOOL "" FORCE)
    set(SDL2_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
    set(SDL2_DISABLE_UNINSTALL ON CACHE BOOL "" FORCE)
    # Reproducible builds: do not pick up ccache implicitly (it also avoids an old
    # cmake_minimum_required(3.4...3.5) call in SDL that warns on CMake 4).
    set(SDL_CCACHE OFF CACHE BOOL "" FORCE)
    # src/main.cpp defines a plain main() and does not include SDL.h, so SDL2main is unused.
    set(SDL2_DISABLE_SDL2MAIN ON CACHE BOOL "" FORCE)
    if(MSVC)
        # Build SDL against the C runtime (as vcpkg does for static SDL2). With
        # SDL_FORCE_STATIC_VCRT, SDL keeps a CMAKE_MSVC_RUNTIME_LIBRARY that is already
        # defined (ours, above) and only defaults it to /MT(d) otherwise.
        set(SDL_LIBC ON CACHE BOOL "" FORCE)
        set(SDL_FORCE_STATIC_VCRT ON CACHE BOOL "" FORCE)
    endif()

    # SYSTEM: SDL's include directories are treated as system headers in our targets, so
    # SDL's headers never trigger our warning flags (or -Werror / /WX).
    FetchContent_Declare(SDL2
        URL      ${NEOWOLF_SDL2_URL}
        URL_HASH SHA256=${NEOWOLF_SDL2_SHA256}
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        SYSTEM
    )

    # CMake 4.x note: SDL 2.32 declares cmake_minimum_required(VERSION 3.0.0...3.10). The
    # "...3.10" upper bound sets its policy level to 3.10, so it configures on CMake 4
    # without CMAKE_POLICY_VERSION_MINIMUM (verified with CMake 4.5). If a future tag fails
    # with "Compatibility with CMake < 3.5 has been removed", set
    # CMAKE_POLICY_VERSION_MINIMUM=3.5 just around this call and unset it afterwards.
    #
    # MSVC runtime: SDL 2.32.10's CMakeLists.txt sets CMAKE_POLICY_DEFAULT_CMP0091 to NEW
    # itself before its cmake_minimum_required(), so it honours CMAKE_MSVC_RUNTIME_LIBRARY
    # and builds with the same static CRT (/MT, /MTd for Debug) as our targets.
    FetchContent_MakeAvailable(SDL2)

    if(NOT TARGET SDL2::SDL2-static)
        message(FATAL_ERROR "The fetched SDL2 ${NEOWOLF_SDL2_VERSION} did not provide SDL2::SDL2-static.")
    endif()
    target_link_libraries(neowolf_sdl2 INTERFACE SDL2::SDL2-static)
    message(STATUS "Neo-Wolf: using static SDL2 ${NEOWOLF_SDL2_VERSION} built from source (${sdl2_SOURCE_DIR})")
endif()

# main() is a plain C++ main (src/main.cpp and tests/LogicTests.cpp). SDL_MAIN_HANDLED stops
# <SDL.h> from renaming main to SDL_main on Windows, so SDL2main is never needed.
target_compile_definitions(neowolf_sdl2 INTERFACE SDL_MAIN_HANDLED)

# --- Warning flags (our targets only; SDL is never affected) ------------------------------
add_library(neowolf_warnings INTERFACE)
if(MSVC)
    target_compile_options(neowolf_warnings INTERFACE /W4 /permissive- /utf-8 /EHsc /Zc:__cplusplus)
    if(NEOWOLF_WERROR)
        target_compile_options(neowolf_warnings INTERFACE /WX)
    endif()
else()
    target_compile_options(neowolf_warnings INTERFACE -Wall -Wextra -Wpedantic)
    if(NEOWOLF_WERROR)
        target_compile_options(neowolf_warnings INTERFACE -Werror)
    endif()
endif()

# --- nlohmann/json (vendored, header-only) -----------------------------------------------
# third_party/nlohmann/json.hpp, version and checksum in third_party/README.md. SYSTEM: the
# header is included as a system header, so it never triggers our warning flags (or -Werror /
# /WX; CMake passes it to MSVC with /external:I and /external:W0).
add_library(neowolf_json INTERFACE)
target_include_directories(neowolf_json SYSTEM INTERFACE ${PROJECT_SOURCE_DIR}/third_party)

# --- Engine library ----------------------------------------------------------------------
add_library(neowolf_engine STATIC
    engine/assets/AssetManager.cpp
    engine/assets/AudioConfig.cpp
    engine/assets/CampaignConfig.cpp
    engine/assets/ContentConfig.cpp
    engine/assets/GameplayConfig.cpp
    engine/assets/Texture.cpp
    engine/assets/TextureLoader.cpp
    engine/audio/AudioSystem.cpp
    engine/core/App.cpp
    engine/core/CommandLine.cpp
    engine/core/Engine.Campaign.cpp
    engine/core/Engine.Core.cpp
    engine/core/Engine.Init.cpp
    engine/core/Engine.Render.cpp
    engine/core/Engine.Shutdown.cpp
    engine/core/Engine.Update.cpp
    engine/core/GameFlow.cpp
    engine/core/JsonDocument.cpp
    engine/core/Log.cpp
    engine/core/Paths.cpp
    engine/core/Progress.cpp
    engine/interop/CommandBridge.cpp
    engine/interop/EngineInterop.cpp
    engine/particles/Particle.cpp
    engine/platform/PlatformSDL.cpp
    engine/render/Framebuffer.cpp
    engine/render/ParticleRenderer.cpp
    engine/render/Raycaster.cpp
    engine/render/SpriteRenderer.cpp
    engine/ui/UiRenderer.cpp
    engine/world/EnemyAi.cpp
    engine/world/Entity.cpp
    engine/world/EntityPlacementLoader.cpp
    engine/world/InteractionRules.cpp
    engine/world/LevelBuilder.cpp
    engine/world/Map.cpp
    engine/world/MapLoader.cpp
    engine/world/MovementRules.cpp
    engine/world/Navigation.cpp
)
target_include_directories(neowolf_engine PUBLIC ${PROJECT_SOURCE_DIR})
target_compile_features(neowolf_engine PUBLIC cxx_std_17)
target_link_libraries(neowolf_engine
    PUBLIC  neowolf_sdl2
    PRIVATE neowolf_json neowolf_warnings)
# `neo_wolf --version` prints this (engine/core/CommandLine.cpp).
target_compile_definitions(neowolf_engine PRIVATE NEOWOLF_VERSION="${PROJECT_VERSION}")

# --- Game executable -----------------------------------------------------------------------
add_executable(neo_wolf src/main.cpp)
target_link_libraries(neo_wolf PRIVATE neowolf_engine neowolf_warnings)
if(WIN32)
    # Release-type builds are GUI-subsystem apps (no console window); Debug keeps the console
    # for log output. main() stays a plain main(): MSVC needs the console CRT entry point for
    # that (MinGW's -mwindows runtime calls main() by itself). src/main.cpp attaches to the
    # parent console for --help/--version, and uses CommandLineToArgvW (shell32) for UTF-8 argv.
    set_target_properties(neo_wolf PROPERTIES WIN32_EXECUTABLE "$<NOT:$<CONFIG:Debug>>")
    if(MSVC)
        target_link_options(neo_wolf PRIVATE "$<$<NOT:$<CONFIG:Debug>>:/ENTRY:mainCRTStartup>")
    endif()
    target_link_libraries(neo_wolf PRIVATE shell32)
endif()

# --- Tests -------------------------------------------------------------------------------
if(NEOWOLF_BUILD_TESTS)
    enable_testing()

    add_executable(neo_wolf_tests tests/LogicTests.cpp)
    target_link_libraries(neo_wolf_tests PRIVATE neowolf_engine neowolf_warnings)
    target_compile_definitions(neo_wolf_tests PRIVATE
        NW_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
        NW_EXPECTED_VERSION="${PROJECT_VERSION}")

    # One CTest entry per native test case. Keep this list in sync with the table at the
    # bottom of tests/LogicTests.cpp; `native_test_registry` fails if the two drift apart.
    set(NEOWOLF_NATIVE_TESTS
        vec2_math_helpers
        input_state_transitions
        map_queries
        raycast_hit_distance
        texture_loader_fallback
        entity_manager_basics
        player_collision_rules
        sprite_distance_sorting
        entity_interaction_rules
        content_file_loaders
        audio_config_loader
        asset_manager_fallback_load
        game_state_transitions
        player_combat_shooting
        enemy_ai_behavior
        pickup_collection
        command_bridge_events
        file_logging
        level_reset_dynamic_load
        vertical_slice_simulation
        map_perimeter_reachability
        asset_inventory_resilience
        engine_interop_c_api
        content_root_resolution
        command_line_parsing
        crlf_content_loading
        log_file_rotation
        gameplay_definitions_exact
        gameplay_definitions_invalid
        entity_placements_json_csv
        decorations_and_sprite_sync
        pickup_kind_from_data
        edited_data_changes_game
        level_load_failures
        game_flow_menu
        game_flow_progression
        game_flow_death_pause
        campaign_manifest_validation
        exit_switch_activation
        progress_file_roundtrip
        pref_dir_resolution
        theme_loading_and_swap
        ui_layout_bounds
        campaign_playthrough
        new_game_resets_world
        key_bindings
        wall_texture_orientation
        line_of_sight
        enemy_hearing
        enemy_pathfinding
        enemy_archetypes_from_data
        entity_variant_validation
        locked_doors_and_keys
        door_close_blocked
        corpse_and_hit_flash
        enemy_separation
        hud_key_icons_layout
        ai_stress_20_enemies
    )
    foreach(test_name IN LISTS NEOWOLF_NATIVE_TESTS)
        add_test(NAME native.${test_name}
                 COMMAND neo_wolf_tests ${test_name}
                 WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
        set_tests_properties(native.${test_name} PROPERTIES LABELS native)
    endforeach()

    add_test(NAME native_test_registry
             COMMAND ${CMAKE_COMMAND}
                     -DTEST_EXE=$<TARGET_FILE:neo_wolf_tests>
                     "-DEXPECTED=${NEOWOLF_NATIVE_TESTS}"
                     -P ${PROJECT_SOURCE_DIR}/cmake/CheckTestRegistry.cmake
             WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
    set_tests_properties(native_test_registry PROPERTIES LABELS native)

    # Headless runs of the game itself, with SDL's dummy video and audio drivers. They run
    # from the build directory, so content must be found next to/above the executable, and
    # SDL_RENDER_DRIVER is removed from the environment: the renderer must fall back to
    # software on its own (known issue B11). Every run passes --pref-dir with a folder in the
    # build tree, so tests never touch the player's real log or progress.json (known issue B15).
    set(NEOWOLF_HEADLESS_ENV
        "SDL_VIDEODRIVER=set:dummy"
        "SDL_AUDIODRIVER=set:dummy"
        "SDL_RENDER_DRIVER=unset:"
        "NEOWOLF_DATA_DIR=unset:"
        "NEOWOLF_PREF_DIR=unset:")
    set(NEOWOLF_TEST_PREF_ROOT "${CMAKE_BINARY_DIR}/test-pref")
    set(NEOWOLF_GAME_TESTS "")
    # neowolf_add_game_test(<name> <expected exit code> <regex> <game args...>)
    function(neowolf_add_game_test name expect_exit regex)
        set(game_args ${ARGN} --pref-dir "${NEOWOLF_TEST_PREF_ROOT}/${name}")
        add_test(NAME ${name}
                 COMMAND ${CMAKE_COMMAND}
                         -DGAME_EXE=$<TARGET_FILE:neo_wolf>
                         "-DGAME_ARGS=${game_args}"
                         -DEXPECT_EXIT=${expect_exit}
                         "-DEXPECT_REGEX=${regex}"
                         -P ${PROJECT_SOURCE_DIR}/cmake/RunGame.cmake
                 WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
        set(NEOWOLF_GAME_TESTS ${NEOWOLF_GAME_TESTS} ${name} PARENT_SCOPE)
    endfunction()

    neowolf_add_game_test(game_smoke_frames 0
        "Per-user folder: .*test-pref.*\\(--pref-dir\\).*Content root: .*Gameplay definitions loaded from .*Campaign .* 3 level\\(s\\).*Renderer: .*Level entities: 7 entities.*Ran 120 frame"
        --frames 120)
    set(NEOWOLF_EMPTY_DATA_DIR "${CMAKE_BINARY_DIR}/test-empty-data")
    file(MAKE_DIRECTORY "${NEOWOLF_EMPTY_DATA_DIR}")
    neowolf_add_game_test(game_smoke_missing_data 1 "Game content not found"
        --data "${NEOWOLF_EMPTY_DATA_DIR}" --frames 10)
    # A content root whose gameplay.json is invalid (a string where an integer belongs):
    # startup must fail with exit code 1 and name the bad key (fail fast, Roadmap Phase 4).
    set(NEOWOLF_BAD_GAMEPLAY_DIR "${CMAKE_BINARY_DIR}/test-bad-gameplay")
    file(MAKE_DIRECTORY "${NEOWOLF_BAD_GAMEPLAY_DIR}/game/maps")
    file(WRITE "${NEOWOLF_BAD_GAMEPLAY_DIR}/game/defs/gameplay.json"
         "{ \"weapon\": { \"damage\": \"lots\" } }\n")
    neowolf_add_game_test(game_smoke_invalid_gameplay 1 "weapon\\.damage must be an integer"
        --data "${NEOWOLF_BAD_GAMEPLAY_DIR}" --frames 10)
    # A content root with a valid gameplay.json but an empty level list (Phase 5 fail-fast).
    set(NEOWOLF_BAD_CAMPAIGN_DIR "${CMAKE_BINARY_DIR}/test-bad-campaign")
    file(MAKE_DIRECTORY "${NEOWOLF_BAD_CAMPAIGN_DIR}/game/maps")
    file(WRITE "${NEOWOLF_BAD_CAMPAIGN_DIR}/game/defs/gameplay.json" "{}\n")
    file(WRITE "${NEOWOLF_BAD_CAMPAIGN_DIR}/game/defs/campaign.json" "{ \"title\": \"T\", \"levels\": [] }\n")
    neowolf_add_game_test(game_smoke_invalid_campaign 1 "Invalid campaign manifest.*needs at least one level"
        --data "${NEOWOLF_BAD_CAMPAIGN_DIR}" --frames 10)
    # Every campaign level starts directly with --level N (development/test option).
    foreach(level 1 2 3)
        neowolf_add_game_test(game_smoke_level_${level} 0
            "Starting level ${level} .*Texture theme .* applied.*GameState changed to: Gameplay.*Ran 120 frame"
            --level ${level} --frames 120)
    endforeach()
    neowolf_add_game_test(game_smoke_level_out_of_range 1 "--level 4: the campaign has 3 level"
        --level 4 --frames 10)
    # Screen previews (developer tool): each renders and saves a screenshot.
    foreach(screen menu levelselect hud minimap pause levelcomplete gameover campaigncomplete enemies items)
        neowolf_add_game_test(game_preview_${screen} 0 "Preview: ${screen} .*Screenshot saved"
            --preview ${screen} --frames 5 --screenshot "${CMAKE_BINARY_DIR}/test-shots/${screen}.bmp")
    endforeach()
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/test-shots")
    set_tests_properties(${NEOWOLF_GAME_TESTS} PROPERTIES
        LABELS game
        TIMEOUT 120
        ENVIRONMENT_MODIFICATION "${NEOWOLF_HEADLESS_ENV}")

    find_package(Python3 COMPONENTS Interpreter)
    if(Python3_Interpreter_FOUND)
        add_test(NAME devkit_tests
                 COMMAND ${Python3_EXECUTABLE} tools/devkit/devkit.py --no-color test
                 WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
        add_test(NAME content_validation
                 COMMAND ${Python3_EXECUTABLE} tools/devkit/devkit.py --no-color validate --strict
                 WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
        set_tests_properties(devkit_tests content_validation PROPERTIES LABELS devkit)
    else()
        message(WARNING "Python 3 not found: devkit_tests and content_validation are not registered.")
    endif()
endif()
