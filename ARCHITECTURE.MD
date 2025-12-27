# Micro-Idle (raylib / C) — Architecture

## 1) Goals

- Code-first, minimal dependencies, single-binary runtime.
- GPU-driven simulation + rendering with no CPU fallback.
- Stable frame pacing and predictable performance at scale.
- Determinism-friendly time model (fixed step), even if not always enabled.
- Structure that is safe to edit and extend module-by-module.

---

## 2) Development Requirements

### 2.1 Scope

This section defines mandatory development practices and quality requirements for all code contributions to the Micro-Idle project.

### 2.2 Objective

The primary objective of all development activities MUST be **complexity reduction while achieving functional requirements**. Code clarity, maintainability, and simplicity are first-order concerns, not optional qualities.

### 2.3 Normative Requirements

#### 2.3.1 Root Cause Analysis (Mandatory)

**REQ-001**: When encountering defects, instability, or unintended behavior, developers MUST identify and address the root cause.

**REQ-002**: Developers MUST NOT introduce compensatory mechanisms that mask symptoms, including but not limited to:
- Emergency correction systems triggered by threshold violations
- Arbitrary bounds or clamps applied to suppress extremes
- Out-of-band validation checks compensating for upstream logic errors
- Conditional branches that special-case problematic states

**REQ-003**: If a proposed fix does not address the root cause, it MUST be rejected regardless of whether it suppresses the observable symptom.

**Rationale**: Symptomatic fixes create feedback loops, accumulate technical debt exponentially, and obscure root causes during debugging.

**Example of non-compliance**:
```
// VIOLATION of REQ-002
if (microbe_area < MIN_THRESHOLD) {
    // Emergency inflation
    apply_expansion_force(EMERGENCY_MULTIPLIER);
}
```

**Example of compliance**:
```
// COMPLIANT: Root cause fix
// Moved pressure constraint outside solver loop to eliminate
// oscillation feedback (see commit a3f8c91)
```

#### 2.3.2 Verification (Mandatory)

**REQ-004**: Developers MUST build the project after every logical change unit that modifies source code or build configuration.

**REQ-005**: Developers MUST execute relevant test suites to verify correctness before considering a logic change complete.

**REQ-006**: Developers MUST NOT proceed to subsequent changes without confirming current changes produce expected behavior.

**Note**: Documentation-only changes do not require build verification unless they modify build scripts or configuration.

#### 2.3.3 Continuous Refactoring (Mandatory)

**REQ-007**: Developers MUST actively refactor code to reduce complexity whenever opportunity arises.

**REQ-008**: Each modification MUST reduce overall system complexity or, at minimum, maintain complexity neutrality. Complexity-increasing changes require explicit justification.

**REQ-009**: Developers MUST identify and eliminate code smells during normal development activity, not defer cleanup to future work.

**REQ-010**: Individual source files MUST NOT exceed reasonable size limits. Files approaching 1000 lines MUST be split into logical modules with clear separation of concerns.

#### 2.3.4 Algorithm Selection (Mandatory)

**REQ-011**: When a current implementation approach cannot satisfy requirements, developers MUST replace the algorithm entirely rather than extend it with special cases.

**REQ-012**: Developers MUST utilize established libraries, proven techniques, and reference implementations in preference to custom solutions.

**REQ-013**: Developers MUST consult academic literature, reference implementations, and domain expertise when selecting algorithms.

#### 2.3.5 Problem-Solving Protocol (Mandatory)

**REQ-014**: When an approach fails to achieve requirements, developers MUST:
1. Attempt an alternative algorithm or technique
2. If alternative approaches fail, systematically refactor the surrounding codebase until the root cause or design flaw is identified
3. Address the identified flaw rather than working around it

**REQ-015**: Developers MUST NOT accept persistent failures. If a solution path is not converging, the problem definition or architectural approach is incorrect and MUST be reevaluated.

### 2.4 Code Quality Standards

**REQ-016**: All code MUST prioritize readability and maintainability over cleverness or brevity.

**REQ-017**: Complexity MUST be justified by requirements. Unjustified complexity MUST be eliminated.

**REQ-018**: Every modification MUST leave the codebase in a cleaner state than before the change.

### 2.5 Compliance

Non-compliance with requirements marked "Mandatory" is not permitted. Code review MUST verify adherence to these requirements before merge.

---

## 3) Stack

- **Language:** C11
- **Framework:** raylib 5.5 (window/input/audio/rlgl)
- **Graphics:** OpenGL 4.6 compute + SSBOs
  - We run on **zink** (Mesa Vulkan -> OpenGL) for 4.6 core features.
  - `bin/main.c` sets `MESA_LOADER_DRIVER_OVERRIDE=zink` on Linux.
- **No CPU mode:** The game always runs in GPU mode.

---

## 4) Repository Layout (Contract)

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

## 5) Build & Targets

CMake targets:

- `game` (release/dev runtime)
- `tests` (unit + GPU integration)

Important build settings:

- `GRAPHICS_API_OPENGL_46` enforced at compile time.
- No CPU fallback; GPU sim must initialize or the game exits.
- Coverage target: **95%+** on core modules.
- Build scripts: `bin/build.sh` (configure/build) and `bin/clean.sh` (cache cleanup).

---

## 6) Engine/Platform Layer

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

## 7) GPU-Driven Simulation (Core Requirement)

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

## 8) Rendering & Visuals

- A single instanced draw renders microbes.
- Shaders handle translucency, internal structure cues, and subtle motion.
- Any mesh details and animation are driven by GPU data, not CPU loops.

---

## 9) Tests & Coverage

- Tests live in `/tests` and include GPU integration tests.
- Coverage gate: **95%+** across core modules.
- Coverage script: `bin/check_coverage.sh`.

---

## 10) Data & Assets

- `data/raw` holds source assets.
- `data/shaders` holds GLSL sources.
- `data/baked` holds packed/processed outputs for release.

---

## 11) Roadmap Notes

- `engine/render` and `engine/assets` will expand as visuals and content scale.
- `engine/io` will host VFS and pack reader logic.
- `engine/debug` will grow into live inspection tooling for GPU state.
