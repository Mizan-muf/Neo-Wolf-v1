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

# --- The version: the single source -------------------------------------------------------
# Everything else is derived from it: `neo_wolf --version` and the log, the autoplay report,
# NeoWolf.app's Info.plist, neo_wolf.exe's version resource, the package README.txt and the
# package file names (cmake/Packaging.cmake). Change it here only, in the release commit that
# is tagged v<version> (docs/RELEASE.md, "Versioning"); 1.0.0 is set by Roadmap Phase 12.
project(NeoWolf VERSION 1.0.0 LANGUAGES C CXX)

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
    set(NEOWOLF_SDL2_LINKED_VERSION "${NEOWOLF_SDL2_VERSION}")
    set(NEOWOLF_SDL2_LICENSE_FILE "${sdl2_SOURCE_DIR}/LICENSE.txt")
endif()
if(NOT NEOWOLF_SDL2_LINKED_VERSION)
    set(NEOWOLF_SDL2_LINKED_VERSION "${SDL2_VERSION}")  # system SDL2
endif()

# main() is a plain C++ main (src/main.cpp and tests/LogicTests.cpp). SDL_MAIN_HANDLED stops
# <SDL.h> from renaming main to SDL_main on Windows, so SDL2main is never needed.
target_compile_definitions(neowolf_sdl2 INTERFACE SDL_MAIN_HANDLED)

# --- Warning flags (our targets only; SDL is never affected) ------------------------------
# C/C++ only: neo_wolf also has a Windows resource script (.rc, Phase 9), and the resource
# compilers (rc.exe, windres) do not understand these flags.
add_library(neowolf_warnings INTERFACE)
if(MSVC)
    set(neowolf_warning_flags /W4 /permissive- /utf-8 /EHsc /Zc:__cplusplus)
    if(NEOWOLF_WERROR)
        list(APPEND neowolf_warning_flags /WX)
    endif()
else()
    set(neowolf_warning_flags -Wall -Wextra -Wpedantic)
    if(NEOWOLF_WERROR)
        list(APPEND neowolf_warning_flags -Werror)
    endif()
endif()
target_compile_options(neowolf_warnings INTERFACE "$<$<COMPILE_LANGUAGE:C,CXX>:${neowolf_warning_flags}>")

# --- nlohmann/json (vendored, header-only) -----------------------------------------------
# third_party/nlohmann/json.hpp, version and checksum in third_party/README.md. SYSTEM: the
# header is included as a system header, so it never triggers our warning flags (or -Werror /
# /WX; CMake passes it to MSVC with /external:I and /external:W0).
set(NEOWOLF_JSON_VERSION "3.12.0")  # keep in step with third_party/README.md
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

# --- Autoplay bot (Roadmap Phase 7) --------------------------------------------------------
# A headless bot that plays the real engine through the input-injection seam
# (engine/autoplay/AutoplayBot.h), and its command-line front end neo_wolf_autoplay.
add_library(neowolf_autoplay STATIC
    engine/autoplay/AutoplayBot.cpp
    engine/autoplay/AutoplayReport.cpp
    engine/autoplay/AutoplayTrace.cpp
)
target_link_libraries(neowolf_autoplay
    PUBLIC  neowolf_engine
    PRIVATE neowolf_json neowolf_warnings)

add_executable(neo_wolf_autoplay src/autoplay_main.cpp)
target_link_libraries(neo_wolf_autoplay PRIVATE neowolf_autoplay neowolf_warnings)
if(WIN32)
    target_link_libraries(neo_wolf_autoplay PRIVATE shell32)  # CommandLineToArgvW
endif()

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

# --- Packaging (Roadmap Phase 9) -------------------------------------------------------------
# Python 3 generates the icon and the package README.txt at build time, and runs the DevKit tests.
find_package(Python3 COMPONENTS Interpreter)
include(cmake/Packaging.cmake)

# --- Tests -------------------------------------------------------------------------------
if(NEOWOLF_BUILD_TESTS)
    enable_testing()

    add_executable(neo_wolf_tests tests/LogicTests.cpp)
    target_link_libraries(neo_wolf_tests PRIVATE neowolf_autoplay neowolf_json neowolf_warnings)  # + neowolf_engine (PUBLIC)
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
        game_flow_test_level
        test_level
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
        autoplay_fixture_level
        autoplay_unwinnable_levels
        autoplay_command_line
        player_enemy_collision
        sprite_vertical_crop
        content_parity_fixtures
        light_zones
        render_size_option
        projection_square_pixels
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
    # The campaign's level count, read from campaign.json at configure time (editing the file
    # re-runs configure): the per-level game and autoplay tests below are generated from it.
    set(NEOWOLF_CAMPAIGN_FILE "${PROJECT_SOURCE_DIR}/game/defs/campaign.json")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${NEOWOLF_CAMPAIGN_FILE}")
    file(READ "${NEOWOLF_CAMPAIGN_FILE}" neowolf_campaign_json)
    string(JSON neowolf_campaign_levels ERROR_VARIABLE neowolf_campaign_error LENGTH "${neowolf_campaign_json}" levels)
    if(neowolf_campaign_error)
        message(WARNING "Cannot read the level list from ${NEOWOLF_CAMPAIGN_FILE} (${neowolf_campaign_error}): "
                        "no per-level game_smoke_level_N or autoplay.level.N tests are registered.")
        set(neowolf_campaign_levels 0)
    endif()
    # The test level (campaign.json "testLevel", outside the campaign): with "bootIntoTestLevel"
    # true the game starts in it, so game_smoke_frames sees it instead of level 1.
    string(JSON neowolf_boot_test_level ERROR_VARIABLE neowolf_boot_error GET "${neowolf_campaign_json}" bootIntoTestLevel)
    string(JSON neowolf_test_level_entities ERROR_VARIABLE neowolf_test_level_error
           GET "${neowolf_campaign_json}" testLevel entities)
    if(neowolf_boot_error OR neowolf_test_level_error OR NOT neowolf_boot_test_level)
        set(neowolf_boot_test_level OFF)
    endif()
    # game_smoke_frames starts the first level (campaign level 1, or the test level) and expects
    # its entity count in the log: read it from that level's entity file (a .json) so the test
    # follows the content.
    set(neowolf_first_level_entities_regex "[0-9]+")
    if(neowolf_campaign_levels GREATER 0)
        string(JSON neowolf_first_entities ERROR_VARIABLE neowolf_first_error GET "${neowolf_campaign_json}" levels 0 entities)
        if(neowolf_boot_test_level)
            set(neowolf_first_entities "${neowolf_test_level_entities}")
        endif()
        if(NOT neowolf_first_error AND neowolf_first_entities MATCHES "\\.json$"
           AND EXISTS "${PROJECT_SOURCE_DIR}/${neowolf_first_entities}")
            set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/${neowolf_first_entities}")
            file(READ "${PROJECT_SOURCE_DIR}/${neowolf_first_entities}" neowolf_first_entities_json)
            string(JSON neowolf_first_entity_count ERROR_VARIABLE neowolf_first_error LENGTH "${neowolf_first_entities_json}" entities)
            if(NOT neowolf_first_error)
                set(neowolf_first_level_entities_regex "${neowolf_first_entity_count}")
            endif()
        endif()
    endif()
    # The number of levels the finished campaign must have (Roadmap Phase 8):
    # content_validation_campaign_size runs `devkit validate --strict --require-levels N`.
    set(NEOWOLF_REQUIRED_CAMPAIGN_LEVELS 10 CACHE STRING "Number of levels game/defs/campaign.json must have")
    set(NEOWOLF_HEADLESS_ENV
        "SDL_VIDEODRIVER=set:dummy"
        "SDL_AUDIODRIVER=set:dummy"
        "SDL_RENDER_DRIVER=unset:"
        "NEOWOLF_DATA_DIR=unset:"
        "NEOWOLF_PREF_DIR=unset:")
    set(NEOWOLF_TEST_PREF_ROOT "${CMAKE_BINARY_DIR}/test-pref")
    set(NEOWOLF_GAME_TESTS "")
    # neowolf_add_game_test(<name> <expected exit code> <regex> <game args...>
    #                       [NO_REGEX <regex>] [NO_PROGRESS] [SCREENSHOT <file.bmp> <width>x<height>])
    # NO_REGEX fails the test when the output matches it. NO_PROGRESS fails it when the run
    # writes progress.json in its per-user folder. SCREENSHOT passes --screenshot <file.bmp>
    # and checks the saved image's size.
    function(neowolf_add_game_test name expect_exit regex)
        cmake_parse_arguments(PARSE_ARGV 3 arg "NO_PROGRESS" "NO_REGEX" "SCREENSHOT")
        set(game_args ${arg_UNPARSED_ARGUMENTS} --pref-dir "${NEOWOLF_TEST_PREF_ROOT}/${name}")
        set(extra_checks "")
        if(arg_NO_REGEX)
            list(APPEND extra_checks "-DEXPECT_NO_REGEX=${arg_NO_REGEX}")
        endif()
        if(arg_NO_PROGRESS)
            list(APPEND extra_checks "-DEXPECT_NO_FILE=${NEOWOLF_TEST_PREF_ROOT}/${name}/progress.json")
        endif()
        if(arg_SCREENSHOT)
            list(GET arg_SCREENSHOT 0 screenshot_file)
            list(GET arg_SCREENSHOT 1 screenshot_size)
            list(APPEND game_args --screenshot "${screenshot_file}")
            list(APPEND extra_checks "-DEXPECT_BMP=${screenshot_file}" "-DEXPECT_BMP_SIZE=${screenshot_size}")
        endif()
        add_test(NAME ${name}
                 COMMAND ${CMAKE_COMMAND}
                         -DGAME_EXE=$<TARGET_FILE:neo_wolf>
                         "-DGAME_ARGS=${game_args}"
                         -DEXPECT_EXIT=${expect_exit}
                         "-DEXPECT_REGEX=${regex}"
                         ${extra_checks}
                         -P ${PROJECT_SOURCE_DIR}/cmake/RunGame.cmake
                 WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
        set(NEOWOLF_GAME_TESTS ${NEOWOLF_GAME_TESTS} ${name} PARENT_SCOPE)
    endfunction()

    neowolf_add_game_test(game_smoke_frames 0
        "Per-user folder: .*test-pref.*\\(--pref-dir\\).*Content root: .*Gameplay definitions loaded from .*Campaign .* ${neowolf_campaign_levels} level\\(s\\).*Renderer: .*Level entities: ${neowolf_first_level_entities_regex} entities.*Ran 120 frame"
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
    if(neowolf_campaign_levels GREATER 0)
        foreach(level RANGE 1 ${neowolf_campaign_levels})
            neowolf_add_game_test(game_smoke_level_${level} 0
                "Starting level ${level} .*Texture theme .* applied.*GameState changed to: Gameplay.*Ran 120 frame"
                --level ${level} --frames 120)
        endforeach()
    endif()
    # The test level: the game starts in it (only with bootIntoTestLevel true); --level and
    # --preview still start where they say.
    if(neowolf_boot_test_level)
        neowolf_add_game_test(game_test_level_boot 0
            "Test level: .*bootIntoTestLevel is on.*Booting into the test level.*Starting the test level .*GameState changed to: Gameplay.*Ran 30 frame"
            --frames 30 NO_PROGRESS)
    endif()
    neowolf_add_game_test(game_test_level_not_with_level 0
        "Starting level 1 .*GameState changed to: Gameplay.*Ran 30 frame"
        --level 1 --frames 30 NO_REGEX "Booting into the test level|Starting the test level")
    neowolf_add_game_test(game_test_level_not_with_preview 0 "Preview: hud .*Screenshot saved"
        --preview hud --frames 5 NO_REGEX "Booting into the test level|Starting the test level"
        SCREENSHOT "${CMAKE_BINARY_DIR}/test-shots/test-level-preview-hud.bmp" 640x360)
    math(EXPR neowolf_level_past_end "${neowolf_campaign_levels} + 1")
    neowolf_add_game_test(game_smoke_level_out_of_range 1
        "--level ${neowolf_level_past_end}: the campaign has ${neowolf_campaign_levels} level"
        --level ${neowolf_level_past_end} --frames 10)
    # Screen previews (developer tool): each renders and saves a screenshot at the default
    # render size.
    foreach(screen menu levelselect hud minimap pause levelcomplete gameover campaigncomplete enemies items)
        neowolf_add_game_test(game_preview_${screen} 0 "Preview: ${screen} .*Screenshot saved"
            --preview ${screen} --frames 5
            SCREENSHOT "${CMAKE_BINARY_DIR}/test-shots/${screen}.bmp" 640x360)
    endforeach()
    # --render-size: a valid size is used for the frame and the screenshot; an invalid one is a
    # usage error (exit code 2) that names the limits.
    neowolf_add_game_test(game_render_size 0 "Render size: 320x240\\..*Screenshot saved"
        --level 1 --frames 5 --render-size 320x240
        SCREENSHOT "${CMAKE_BINARY_DIR}/test-shots/render-size-320x240.bmp" 320x240)
    neowolf_add_game_test(game_render_size_invalid 2
        "--render-size needs a size WIDTHxHEIGHT: an even width from 320 to 1920, .* from 5:4 to 64:27"
        --render-size 641x360 --frames 5)
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/test-shots")
    set_tests_properties(${NEOWOLF_GAME_TESTS} PROPERTIES
        LABELS game
        TIMEOUT 120
        ENVIRONMENT_MODIFICATION "${NEOWOLF_HEADLESS_ENV}")

    # Autoplay (Roadmap Phase 7): one test per campaign level, the whole campaign with
    # carry-over, and a deliberately unwinnable fixture level that must fail with a reason.
    set(NEOWOLF_AUTOPLAY_REPORTS "${CMAKE_BINARY_DIR}/autoplay")
    set(NEOWOLF_AUTOPLAY_TESTS "")
    # neowolf_add_autoplay_test(<name> <expected exit code> <regex> <autoplay args...>)
    function(neowolf_add_autoplay_test name expect_exit regex)
        set(autoplay_args ${ARGN} --pref-dir "${NEOWOLF_TEST_PREF_ROOT}/${name}")
        add_test(NAME ${name}
                 COMMAND ${CMAKE_COMMAND}
                         -DGAME_EXE=$<TARGET_FILE:neo_wolf_autoplay>
                         "-DGAME_ARGS=${autoplay_args}"
                         -DEXPECT_EXIT=${expect_exit}
                         "-DEXPECT_REGEX=${regex}"
                         -P ${PROJECT_SOURCE_DIR}/cmake/RunGame.cmake
                 WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
        set(NEOWOLF_AUTOPLAY_TESTS ${NEOWOLF_AUTOPLAY_TESTS} ${name} PARENT_SCOPE)
    endfunction()
    if(neowolf_campaign_levels GREATER 0)
        foreach(level RANGE 1 ${neowolf_campaign_levels})
            math(EXPR level_index "${level} - 1")
            string(JSON level_id ERROR_VARIABLE level_error GET "${neowolf_campaign_json}" levels ${level_index} id)
            if(level_error)
                set(level_id "level ${level}")
            endif()
            neowolf_add_autoplay_test(autoplay.level.${level} 0
                "${level_id} +complete.*Result: PASS"
                --level ${level} --data "${PROJECT_SOURCE_DIR}"
                --json "${NEOWOLF_AUTOPLAY_REPORTS}/level-${level}.json")
        endforeach()
    endif()
    neowolf_add_autoplay_test(autoplay.campaign 0
        "Carry-over: OK.*Result: PASS \\(${neowolf_campaign_levels} of ${neowolf_campaign_levels} level\\(s\\) completed, campaign complete\\)"
        --all --data "${PROJECT_SOURCE_DIR}" --json "${NEOWOLF_AUTOPLAY_REPORTS}/campaign.json")
    neowolf_add_autoplay_test(autoplay.fixture_unwinnable 1
        "unwinnable +stuck.*no route to an exit switch.*Result: FAIL"
        --level 1 --data "${PROJECT_SOURCE_DIR}/tests/fixtures/unwinnable"
        --json "${NEOWOLF_AUTOPLAY_REPORTS}/fixture-unwinnable.json")
    # Phase 8 regression: an idle enemy whose centre is in sight but whose shot grazes a corner
    # must not hold the bot (it gives up on it after 2 s); the old bot needed 33 s here.
    neowolf_add_autoplay_test(autoplay.fixture_aim_blocked 0
        "aim_blocked +complete.*Result: PASS"
        --level 1 --data "${PROJECT_SOURCE_DIR}/tests/fixtures/aim_blocked" --time-limit 15
        --json "${NEOWOLF_AUTOPLAY_REPORTS}/fixture-aim-blocked.json")
    # --trace (event log): campaign level 3 (a key, a locked door, kills, the exit) played twice;
    # the events must be there and the two traces byte-identical (cmake/CheckAutoplayTrace.cmake).
    # Plus the error paths: a trace file that cannot be created (exit 1) and --trace without a
    # value (exit 2).
    if(neowolf_campaign_levels GREATER_EQUAL 3)
        string(JSON trace_level_id ERROR_VARIABLE trace_level_error GET "${neowolf_campaign_json}" levels 2 id)
        if(trace_level_error)
            set(trace_level_id "[a-z0-9_-]+")
        endif()
        set(trace_events
            "^L3 t=0\\.000 f=0 level-start id=${trace_level_id} hp=[0-9]+ ammo=[0-9]+ "
            "^L3 .* enemy [a-z]+#[0-9]+ cell=[0-9]+,[0-9]+ idle->alert$"
            "^L3 .* hit-dealt target=[a-z]+#[0-9]+ cell=[0-9]+,[0-9]+ damage=[0-9]+ hp=0/[0-9]+ killed$"
            "^L3 .* enemy [a-z]+#[0-9]+ cell=[0-9]+,[0-9]+ [a-z]+->dead$"
            "^L3 .* key colour=[a-z]+ cell=[0-9]+,[0-9]+ held="
            "^L3 .* door-open cell=[0-9]+,[0-9]+ lock=[a-z]+ unlocked$"
            "^L3 .* exit cell=[0-9]+,[0-9]+ pos="
            "^L3 .* level-end result=complete ")
        add_test(NAME autoplay.trace
                 COMMAND ${CMAKE_COMMAND}
                         -DGAME_EXE=$<TARGET_FILE:neo_wolf_autoplay>
                         "-DGAME_ARGS=--level;3;--data;${PROJECT_SOURCE_DIR}"
                         "-DTRACE_DIR=${NEOWOLF_AUTOPLAY_REPORTS}/trace"
                         "-DEXPECT_EVENTS=${trace_events}"
                         -P ${PROJECT_SOURCE_DIR}/cmake/CheckAutoplayTrace.cmake
                 WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
        list(APPEND NEOWOLF_AUTOPLAY_TESTS autoplay.trace)
    endif()
    neowolf_add_autoplay_test(autoplay.trace_unwritable 1 "cannot write the trace to "
        --level 1 --data "${PROJECT_SOURCE_DIR}" --trace "${CMAKE_BINARY_DIR}/CMakeCache.txt/trace.txt")
    # Not through neowolf_add_autoplay_test: its --pref-dir would become the value of --trace.
    add_test(NAME autoplay.trace_usage
             COMMAND ${CMAKE_COMMAND}
                     -DGAME_EXE=$<TARGET_FILE:neo_wolf_autoplay>
                     "-DGAME_ARGS=--level;1;--trace"
                     -DEXPECT_EXIT=2
                     "-DEXPECT_REGEX=--trace needs a value"
                     -P ${PROJECT_SOURCE_DIR}/cmake/RunGame.cmake
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    list(APPEND NEOWOLF_AUTOPLAY_TESTS autoplay.trace_usage)
    set_tests_properties(${NEOWOLF_AUTOPLAY_TESTS} PROPERTIES
        LABELS autoplay
        TIMEOUT 300
        ENVIRONMENT_MODIFICATION "${NEOWOLF_HEADLESS_ENV}")

    if(Python3_Interpreter_FOUND)
        add_test(NAME devkit_tests
                 COMMAND ${Python3_EXECUTABLE} tools/devkit/devkit.py --no-color test
                 WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
        add_test(NAME content_validation
                 COMMAND ${Python3_EXECUTABLE} tools/devkit/devkit.py --no-color validate --strict
                 WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
        add_test(NAME content_validation_campaign_size
                 COMMAND ${Python3_EXECUTABLE} tools/devkit/devkit.py --no-color validate --strict
                         --require-levels ${NEOWOLF_REQUIRED_CAMPAIGN_LEVELS}
                 WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
        set_tests_properties(devkit_tests content_validation content_validation_campaign_size PROPERTIES LABELS devkit)
    else()
        message(WARNING "Python 3 not found: devkit_tests, content_validation and content_validation_campaign_size are not registered.")
    endif()

    # Package tests (Roadmap Phase 9), opt-in because they build a separate Release tree
    # (build/package-<os>/, a few minutes the first time): `cmake --preset macos
    # -DNEOWOLF_PACKAGE_TESTS=ON`, then `ctest --preset macos -L package`. package.build runs the
    # DevKit `package` command into build/<preset>/package-test/; package.smoke checks the result
    # with tools/ci/package_smoke.py (extract to a path with a space, run headless, cwd-independence,
    # signature/architectures/libraries on macOS). CI runs the same two steps (job `package`).
    option(NEOWOLF_PACKAGE_TESTS "Register the slow package.build and package.smoke CTest tests" OFF)
    if(NEOWOLF_PACKAGE_TESTS AND Python3_Interpreter_FOUND)
        set(NEOWOLF_PACKAGE_TEST_DIR "${CMAKE_BINARY_DIR}/package-test")
        add_test(NAME package.build
                 COMMAND ${Python3_EXECUTABLE} tools/devkit/devkit.py --no-color package
                         --out-dir "${NEOWOLF_PACKAGE_TEST_DIR}" --clean
                 WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
        add_test(NAME package.smoke
                 COMMAND ${Python3_EXECUTABLE} tools/ci/package_smoke.py "${NEOWOLF_PACKAGE_TEST_DIR}"
                 WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
        set_tests_properties(package.build PROPERTIES FIXTURES_SETUP neowolf_package)
        set_tests_properties(package.smoke PROPERTIES FIXTURES_REQUIRED neowolf_package)
        set_tests_properties(package.build package.smoke PROPERTIES
            LABELS package
            TIMEOUT 1800
            ENVIRONMENT_MODIFICATION "SDL_VIDEODRIVER=set:dummy;SDL_AUDIODRIVER=set:dummy;NEOWOLF_DATA_DIR=unset:;NEOWOLF_PREF_DIR=unset:")
    endif()
endif()
