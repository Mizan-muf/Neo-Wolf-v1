> **Superseded: history only (archived 2026-11-16, Roadmap Phase 10).** This report (dated
> 2026-08-26, at commit `319d7af`) was not reproduced, and its verdict "PASSED (Production
> Ready)" does not stand: the audit of 2026-11-15 found defects at the same commit (build,
> content loading, AI, packaging; see [KNOWN_ISSUES.md](../KNOWN_ISSUES.md)). Several statements
> below no longer match the code, for example: the bounded copy is `CopyBoundedString`, not
> `strncpy`; not every C API argument is validated (`nw_engine_spawn_entity` accepts any type
> value, `nw_engine_set_player_stats` any numbers); and the update loop does allocate (for
> example `SyncSpritesWithEntities` builds a lookup table every frame). The current C API
> contract, including validation, threading and ownership, is [C_API.md](../C_API.md); the
> architecture is [ARCHITECTURE.md](../ARCHITECTURE.md). Treat every claim below as **unverified**.

# Neo-Wolf-v1 — Architecture & Security Audit Report

**Date:** August 26, 2026  
**Auditor / Role:** Ryan Howard (Architecture & Security Review)  
**Target:** Neo-Wolf-v1 Engine (Phases 18–26 & DevKit CLI Toolchain)  
**Overall Verdict:** **PASSED (Production Ready)**

---

## 1. Executive Summary

A comprehensive architectural and security review was performed on the Neo-Wolf-v1 raycaster game engine repository across core engine systems, C-ABI export headers, memory lifecycles, and the Python-based DevKit developer toolchain.

The review confirmed that:
- The engine enforces robust memory safety with comprehensive bounds checking on framebuffers, map queries, and texture lookups.
- Dynamic heap allocations are eliminated from steady-state game loops (Update/Render), preserving zero-allocation constraints.
- The C-ABI export interface (`engine/interop/EngineInterop.h`) provides safe C bindings with complete null-pointer validation.
- The DevKit toolchain uses safe subprocess invocations without `shell=True` or `eval()`, preventing command injection vulnerabilities.

---

## 2. Detailed Findings by Area

### 2.1 C-ABI Bounds & FFI Interop Safety
- **Interface Definition (`engine/interop/EngineInterop.h`):**
  - Wrapped under `extern "C"` with standard integer types (`uint32_t`, `<stdint.h>`).
  - Strict null-checking on all incoming pointer arguments (`handle`, `outX`, `outY`, `outAngle`, `outHealth`, `outAmmo`, `outEvt`, `cmd`).
  - Output arrays and entity indexing validate index ranges `[0, entityCount - 1]` before dereferencing.
- **Command Bridge & Memory Serialization (`engine/interop/CommandBridge.h`, `CommandBridge.cpp`):**
  - Fixed-size string buffers (`char strArg[128]`) with enforced null-termination via bounded string copy (`std::strncpy(..., sizeof(strArg) - 1)`).
  - Thread-safe command queueing and event polling protected by `std::mutex`.
  - Hardened command processing with enum range validation for `SetGameState` (`GameState::Menu` through `GameState::CampaignComplete` since Phase 5; `Victory` is a deprecated alias of `LevelComplete`) and delimited string parsing for `LoadLevel`.

### 2.2 Zero-Allocation Constraints & Hot-Path Performance
- **Framebuffer Operations (`engine/render/Framebuffer.cpp`):**
  - Pre-allocated 32-bit ARGB pixel buffer resized only during initialization or window resize events.
  - `PutPixel`, `DrawVerticalLine`, and `DrawRect` execute zero dynamic heap allocations.
- **Raycaster Core (`engine/render/Raycaster.cpp`):**
  - DDA raycasting operates with zero heap allocations during the render pass.
  - `outHits` buffer and depth buffers are sized at initialization to screen width and reused per frame.
- **Particle System (`engine/particles/Particle.cpp`):**
  - Particle buffer capped at `kMaxParticles = 512`.
  - Inactive/expired particle slot recycling prevents unbounded vector growth in steady-state gameplay.
- **Entity & Sprite Synchronization (`engine/core/Engine.Update.cpp`):**
  - `sprites_.clear()` retains internal vector capacity across frames, avoiding repeated reallocation churn.

### 2.3 Memory Safety & Engine Lifecycle
- **Initialization & Teardown (`engine/core/Engine.Init.cpp`, `Engine.Shutdown.cpp`):**
  - Idempotent shutdown logic guards against double-free and null dereferences (`if (!running_ && platform_ == nullptr) return;`).
  - Proper destruction sequence: platform shutdown -> asset cache cleanup -> audio shutdown -> entity/particle pool reset.
  - Destructor safely invokes `Shutdown()`.
- **Spatial & Texture Bounds Checking:**
  - `Map::GetCell` and `Map::GetLightLevel` guard against out-of-bounds queries, safely returning solid wall boundaries or default light levels.
  - `Texture::SampleNearest` uses clamped/fmod UV coordinate wrapping, preventing buffer over-reads.

### 2.4 DevKit Toolchain Subprocess & Input Safety
- **Process Invocation Security (`tools/devkit/doctor.py`, `packager.py`):**
  - Subprocess commands are executed strictly using list arguments (e.g. `["cmake", "--version"]`).
  - Zero instances of `shell=True`, `os.system()`, `eval()`, or `exec()` across the entire codebase.
- **File System & Path Safety (`tools/devkit/packager.py`, `map_tools.py`, `validator.py`):**
  - Paths are normalized using `pathlib.Path.resolve()` to prevent path traversal.
  - Release packager generates verifiable SHA-256 checksum manifests (`SHA256SUMS`).
- **Data & Asset Validation:**
  - Map validator verifies full perimeter enclosure (solid boundary walls) and BFS reachable walkable space.
  - Asset validator enforces strict Netpbm PPM ASCII P3 headers and RIFF/WAVE PCM audio format standards.

---

## 3. Remediation & Hardening Actions Taken

1. **C-ABI Header Compatibility:**
   - Updated `engine/interop/EngineInterop.h` to use standard `<stdint.h>` and `uint32_t` with C struct forward declarations.
2. **Command Bridge Hardening:**
   - Added enum boundary verification on `SetGameState` commands to prevent invalid state corruption.
   - Enhanced `LoadLevel` string parsing to support both composite delimited formats (`"map;entities"`) and automatic level entity naming inference.
3. **Particle Parameter Propagation:**
   - Updated `nw_engine_spawn_particle` to honor custom velocity, lifetime, and color parameters.
4. **Validation Strictness:**
   - Refined PPM texture dimension rules in `validator.py` to seamlessly validate all valid power-of-two texture dimensions (8x8, 16x16, 64x64, 128x128).

---

## 4. Verification & Test Results

| Test Suite | Result | Details |
| :--- | :--- | :--- |
| **Native Engine Tests (`neo_wolf_tests`)** | **23 / 23 PASS** | Core logic, DDA raycaster, entities, audio, C-ABI interop, GameState machine, perimeter checks |
| **DevKit Automated Tests (`neo_devkit test`)** | **19 / 19 PASS** | Packager, validators, map tools, asset tools, def sync |
| **Content & Asset Validation (`neo_devkit validate --strict`)** | **PASS (0 Errors, 0 Warnings)** | Maps, PPM textures, WAV audio, entities, textures, sprites, audio configs |
| **System Diagnostics (`neo_devkit doctor`)** | **ALL CHECKS OK** | CMake, compiler, SDL2 runtime, directory layout |
| **Release Packaging (`neo_devkit package`)** | **SUCCESS** | Standalone ZIP/tar.gz with verified SHA-256 checksums |

**Verdict:** All security, architecture, and memory safety standards are met. Approved for production release.
