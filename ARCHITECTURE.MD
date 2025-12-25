# Micro-Idle (raylib / C) — Architecture

## 1) Goals

- Code-first, minimal dependencies, single-binary runtime.
- GPU-driven simulation + rendering with no CPU fallback.
- Stable frame pacing and predictable performance at scale.
- Determinism-friendly time model (fixed step), even if not always enabled.
- Structure that is safe to edit and extend module-by-module.

---

## 2) Stack

- **Language:** C11
- **Framework:** raylib 5.5 (window/input/audio/rlgl)
- **Graphics:** OpenGL 4.6 compute + SSBOs
  - We run on **zink** (Mesa Vulkan -> OpenGL) for 4.6 core features.
  - `bin/main.c` sets `MESA_LOADER_DRIVER_OVERRIDE=zink` on Linux.
- **No CPU mode:** The game always runs in GPU mode.

---

## 3) Repository Layout (Contract)

```
/engine
  /platform             # raylib-facing platform wrappers: window, input mapping, timing
  /render               # renderer abstraction on top of raylib (passes, layers, materials)
  /assets               # asset manager + pack reader + caching
  /io                   # vfs, serialization, binary formats, compression
  /math                 # vec/mat/quats, color utils, hashing helpers
  /util                 # logging, asserts, allocators, profiling, arenas
  /debug                # debug draw, dev console, capture tools, inspector UI
  /ecs                  # (optional) Flecs integration or custom entity store
/game                   # project-specific code
/tests                  # unit and integration tests
/third_party            # raylib, optional libs
/data
  /raw                  # source assets (png, wav, fonts, models)
  /shaders              # shader sources (glsl)
  /baked                # baked outputs (packed archive, atlases, preprocessed shaders)
/bin                    # build/run/pack scripts
CMakeLists.txt
ARCHITECTURE.md
```

---

## 4) Build & Targets

CMake targets:

- `game` (release/dev runtime)
- `tests` (unit + GPU integration)

Important build settings:

- `GRAPHICS_API_OPENGL_46` enforced at compile time.
- No CPU fallback; GPU sim must initialize or the game exits.
- Coverage target: **95%+** on core modules.
- Build scripts: `bin/build.sh` (configure/build) and `bin/clean.sh` (cache cleanup).

---

## 5) Engine/Platform Layer

`engine/platform` wraps minimal platform responsibilities:

- `EngineConfig` and `EngineContext`
- `time_*` fixed-step accumulator

No global state. Callers wire state explicitly:

```c
typedef struct EngineConfig {
    int window_w, window_h;
    int target_fps;
    int tick_hz;
    bool vsync;
    bool dev_mode;
} EngineConfig;

typedef struct EngineContext {
    EngineConfig cfg;
    TimeState time;
} EngineContext;
```

---

## 6) GPU-Driven Simulation (Core Requirement)

The **only** simulation path is GPU-based. CPU work is limited to input, high-level orchestration, and UI.

- **Entity state** lives in SSBOs (position, velocity, size, type).
- **Compute shaders** update movement and collisions.
- **Spatial grid** (head + next buffers) is built on GPU for broad-phase.
- **Render** uses instanced drawing with SSBO-backed attributes.

The CPU never iterates over all entities per-frame.

---

## 7) Rendering & Visuals

- A single instanced draw renders microbes.
- Shaders handle translucency, internal structure cues, and subtle motion.
- Any mesh details and animation are driven by GPU data, not CPU loops.

---

## 8) Tests & Coverage

- Tests live in `/tests` and include GPU integration tests.
- Coverage gate: **95%+** across core modules.
- Coverage script: `bin/check_coverage.sh`.

---

## 9) Data & Assets

- `data/raw` holds source assets.
- `data/shaders` holds GLSL sources.
- `data/baked` holds packed/processed outputs for release.

---

## 10) Roadmap Notes

- `engine/render` and `engine/assets` will expand as visuals and content scale.
- `engine/io` will host VFS and pack reader logic.
- `engine/debug` will grow into live inspection tooling for GPU state.
