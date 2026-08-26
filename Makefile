CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -I. -I/home/mizan/deps/usr/include/SDL2 -I/home/mizan/deps/usr/include
LDFLAGS ?= -L/home/mizan/deps/usr/lib/x86_64-linux-gnu -L/home/mizan/deps/usr/lib
PYTHON ?= python3

SRC_COMMON = \
    engine/core/App.cpp \
    engine/core/Log.cpp \
    engine/core/Engine.Core.cpp \
    engine/core/Engine.Init.cpp \
    engine/core/Engine.Update.cpp \
    engine/core/Engine.Render.cpp \
    engine/core/Engine.Shutdown.cpp \
    engine/assets/AssetManager.cpp \
    engine/assets/AudioConfig.cpp \
    engine/assets/ContentConfig.cpp \
    engine/assets/Texture.cpp \
    engine/assets/TextureLoader.cpp \
    engine/audio/AudioSystem.cpp \
    engine/interop/CommandBridge.cpp \
    engine/interop/EngineInterop.cpp \
    engine/platform/PlatformSDL.cpp \
    engine/particles/Particle.cpp \
    engine/render/Framebuffer.cpp \
    engine/render/ParticleRenderer.cpp \
    engine/render/Raycaster.cpp \
    engine/render/SpriteRenderer.cpp \
    engine/ui/UiRenderer.cpp \
    engine/world/Entity.cpp \
    engine/world/EntityPlacementLoader.cpp \
    engine/world/InteractionRules.cpp \
    engine/world/MapLoader.cpp \
    engine/world/Map.cpp \
    engine/world/MovementRules.cpp

SRC_GAME = src/main.cpp $(SRC_COMMON)
SRC_TESTS = tests/LogicTests.cpp $(SRC_COMMON)

SDL_LIBS := -lSDL2

.PHONY: all game tests check test validate validate-strict doctor package clean

all: game tests

game: neo_wolf

neo_wolf: $(SRC_GAME)
	@echo "Compiling neo_wolf executable..."
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(SDL_LIBS)

tests: neo_wolf_tests

neo_wolf_tests: $(SRC_TESTS)
	@echo "Compiling neo_wolf_tests executable..."
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(SDL_LIBS)

check: test

test: neo_wolf_tests
	@echo "=== Running Native Engine Tests ==="
	./neo_wolf_tests
	@echo "=== Running DevKit Automated Tests ==="
	$(PYTHON) tools/devkit/devkit.py test

validate:
	@echo "=== Running Content & Asset Validation ==="
	$(PYTHON) tools/devkit/devkit.py validate

validate-strict:
	@echo "=== Running Content & Asset Validation (Strict) ==="
	$(PYTHON) tools/devkit/devkit.py validate --strict

doctor:
	@echo "=== Running System & Project Diagnostics ==="
	$(PYTHON) tools/devkit/devkit.py doctor

package:
	@echo "=== Building Release Package ==="
	$(PYTHON) tools/devkit/devkit.py package --version 1.0.0 --out-dir release

clean:
	rm -rf neo_wolf neo_wolf_tests build release *.zip *.tar.gz *.o *.log
