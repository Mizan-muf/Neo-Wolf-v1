cmake_minimum_required(VERSION 3.21)
project(NeoWolf LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_testing()

add_executable(neo_wolf src/main.cpp)
target_sources(neo_wolf PRIVATE
    engine/core/App.cpp
    engine/core/Log.cpp
    engine/core/Engine.Core.cpp
    engine/core/Engine.Init.cpp
    engine/core/Engine.Update.cpp
    engine/core/Engine.Render.cpp
    engine/core/Engine.Shutdown.cpp
    engine/assets/AssetManager.cpp
    engine/assets/AudioConfig.cpp
    engine/assets/ContentConfig.cpp
    engine/assets/Texture.cpp
    engine/assets/TextureLoader.cpp
    engine/audio/AudioSystem.cpp
    engine/interop/CommandBridge.cpp
    engine/interop/EngineInterop.cpp
    engine/platform/PlatformSDL.cpp
    engine/particles/Particle.cpp
    engine/render/Framebuffer.cpp
    engine/render/ParticleRenderer.cpp
    engine/render/Raycaster.cpp
    engine/render/SpriteRenderer.cpp
    engine/ui/UiRenderer.cpp
    engine/world/Entity.cpp
    engine/world/EntityPlacementLoader.cpp
    engine/world/InteractionRules.cpp
    engine/world/MapLoader.cpp
    engine/world/Map.cpp
    engine/world/MovementRules.cpp
)
target_include_directories(neo_wolf PRIVATE ${CMAKE_SOURCE_DIR})

find_library(SDL2_LIB NAMES SDL2 SDL2-2.0 libSDL2 PATHS /home/mizan/deps/usr/lib /usr/lib/x86_64-linux-gnu)
find_path(SDL2_INCLUDE_DIR NAMES SDL.h PATH_SUFFIXES SDL2 PATHS /home/mizan/deps/usr/include /home/mizan/deps/usr/include/SDL2)
if(SDL2_LIB AND SDL2_INCLUDE_DIR)
    target_include_directories(neo_wolf PRIVATE ${SDL2_INCLUDE_DIR})
    target_link_libraries(neo_wolf PRIVATE ${SDL2_LIB} -lpthread -ldl -lm)
else()
    find_package(SDL2 REQUIRED)
    target_include_directories(neo_wolf PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(neo_wolf PRIVATE ${SDL2_LIBRARIES})
endif()

add_executable(neo_wolf_tests
    tests/LogicTests.cpp
    engine/core/Log.cpp
    engine/core/Engine.Core.cpp
    engine/core/Engine.Init.cpp
    engine/core/Engine.Update.cpp
    engine/core/Engine.Render.cpp
    engine/core/Engine.Shutdown.cpp
    engine/interop/CommandBridge.cpp
    engine/interop/EngineInterop.cpp
    engine/platform/PlatformSDL.cpp
    engine/assets/AssetManager.cpp
    engine/assets/AudioConfig.cpp
    engine/assets/ContentConfig.cpp
    engine/assets/Texture.cpp
    engine/assets/TextureLoader.cpp
    engine/audio/AudioSystem.cpp
    engine/interop/CommandBridge.cpp
    engine/particles/Particle.cpp
    engine/render/Framebuffer.cpp
    engine/render/ParticleRenderer.cpp
    engine/render/Raycaster.cpp
    engine/render/SpriteRenderer.cpp
    engine/ui/UiRenderer.cpp
    engine/world/Entity.cpp
    engine/world/EntityPlacementLoader.cpp
    engine/world/InteractionRules.cpp
    engine/world/MapLoader.cpp
    engine/world/Map.cpp
    engine/world/MovementRules.cpp
)
target_include_directories(neo_wolf_tests PRIVATE ${CMAKE_SOURCE_DIR})
if(SDL2_INCLUDE_DIR)
    target_include_directories(neo_wolf_tests PRIVATE ${SDL2_INCLUDE_DIR})
endif()
if(SDL2_LIB)
    target_link_libraries(neo_wolf_tests PRIVATE ${SDL2_LIB} -lpthread -ldl -lm)
endif()

add_test(NAME neo_wolf_tests COMMAND neo_wolf_tests)

# Copy runtime DLLs (e.g., SDL2.dll from vcpkg dynamic builds) next to the exe.
if(MSVC)
    add_custom_command(TARGET neo_wolf POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:neo_wolf>
            $<TARGET_FILE_DIR:neo_wolf>
        COMMAND_EXPAND_LISTS
    )
endif()
