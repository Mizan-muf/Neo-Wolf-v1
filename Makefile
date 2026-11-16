# Neo-Wolf Makefile: a thin convenience wrapper around the CMake presets (macOS and Linux).
# The real build lives in CMakeLists.txt / CMakePresets.json; see docs/BUILD_MACOS.md.
#
#   make            configure + build game and tests (Release)
#   make check      build, then run every test with CTest (native, DevKit, content validation)
#   make CONFIG=Debug check
#   make PRESET=ci-macos check     (any preset name from CMakePresets.json)

PYTHON ?= python3
CONFIG ?= Release

UNAME_S := $(shell uname -s)
ifeq ($(CONFIG),Debug)
  PRESET_SUFFIX := -debug
else
  PRESET_SUFFIX :=
endif
ifeq ($(UNAME_S),Darwin)
  PRESET ?= macos$(PRESET_SUFFIX)
else ifeq ($(UNAME_S),Linux)
  PRESET ?= linux$(PRESET_SUFFIX)
else
  $(error Unsupported OS '$(UNAME_S)'. On Windows use build.cmd or build.ps1)
endif

BUILD_DIR := build/$(PRESET)

.PHONY: all configure game tests check test validate validate-strict doctor package clean

all: configure
	cmake --build --preset $(PRESET)

# Re-running configure is cheap once the cache exists; cmake --build also re-configures
# automatically when CMakeLists.txt changes.
configure:
	cmake --preset $(PRESET)

game: configure
	cmake --build --preset $(PRESET) --target neo_wolf

tests: configure
	cmake --build --preset $(PRESET) --target neo_wolf_tests

check: all
	ctest --preset $(PRESET)

test: check

validate:
	$(PYTHON) tools/devkit/devkit.py validate

validate-strict:
	$(PYTHON) tools/devkit/devkit.py validate --strict

doctor:
	$(PYTHON) tools/devkit/devkit.py doctor

# Packaging is reworked in Roadmap Phase 9. For now the packager picks up the binary from
# build/<preset>/, so build the game first.
package: game
	$(PYTHON) tools/devkit/devkit.py package --version 1.0.0 --out-dir release

# Also removes binaries left in the repository root by the pre-CMake Makefile.
clean:
	rm -rf build release neo_wolf neo_wolf_tests *.zip *.tar.gz *.o *.log
