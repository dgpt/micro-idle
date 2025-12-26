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

### Physics Architecture: XPBD (Extended Position Based Dynamics)

We use **XPBD** for soft-body physics, based on the [Macklin et al. paper](http://mmacklin.com/xpbd.pdf). This provides:
- Stable, iteration-independent stiffness via compliance parameters
- Natural soft-body deformation through constraint solving
- GPU-parallelizable constraint projection

### Two-Layer Simulation Model

```
┌─────────────────────────────────────────────────────────────┐
│  PHYSICS LAYER (Low-res particle clusters)                  │
│  - Each microbe = 8-16 particles in a ring/blob             │
│  - Distance constraints maintain shape                       │
│  - Collisions between microbes via particle proximity        │
│  - XPBD solver runs on GPU compute shaders                  │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼ (particle positions drive deformation)
┌─────────────────────────────────────────────────────────────┐
│  RENDER LAYER (High-res visual mesh)                        │
│  - Smooth visual hull generated from particle positions     │
│  - Vertex shader interpolates between particle positions    │
│  - Fragment shader adds biological detail (membrane, etc.)  │
└─────────────────────────────────────────────────────────────┘
```

### XPBD Algorithm (per frame)

1. **Predict**: Apply external forces, predict new positions
2. **Build spatial grid**: For broad-phase collision detection
3. **Generate collision constraints**: Between nearby particles of different microbes
4. **Solve constraints** (N iterations):
   - For each constraint: compute Δλ = -(C + α̃λ) / (Σw|∇C|² + α̃)
   - Apply position correction: x += w·Δλ·∇C
5. **Update velocities**: v = (x - x_prev) / dt

### Data Layout (SSBOs)

- **Particles**: position, velocity, inverse mass, microbe ID
- **Constraints**: particle indices, rest length, compliance, λ accumulator
- **Microbes**: center of mass, rotation, type, visual parameters
- **Spatial Grid**: head + next buffers for broad-phase

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
