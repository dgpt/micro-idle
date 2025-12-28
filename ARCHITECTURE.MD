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

**REQ-019**: Debug output, logging, screenshots, and diagnostic code MUST NOT be added to production game logic. All debugging and diagnosis MUST be done through proper tests in `/tests`.

### 2.5 Compliance

Non-compliance with requirements marked "Mandatory" is not permitted. Code review MUST verify adherence to these requirements before merge.

---

## 3) Stack

- **Language:** C++17
- **Framework:** raylib 5.5 (window/input/audio/rlgl)
- **Physics:** Bullet Physics 3.25 with OpenCL GPU solver
- **Graphics:** OpenGL 4.6 (rendering/shaders) + SSBOs
  - We run on **zink** (Mesa Vulkan -> OpenGL) for 4.6 core features.
  - `bin/main.cpp` sets `MESA_LOADER_DRIVER_OVERRIDE=zink` on Linux.
- **GPU-accelerated:** Physics runs on GPU via Bullet OpenCL, rendering via OpenGL compute/fragment shaders

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

## 7) GPU-Accelerated Physics & Rendering

Physics simulation runs on GPU via **Bullet Physics OpenCL solver**. Rendering uses OpenGL shaders with GPU instancing. CPU handles high-level orchestration, input, and UI only.

### Physics Architecture: Bullet Soft Body Dynamics

We use **Bullet Physics** soft body simulation with OpenCL GPU acceleration:
- **Proven library**: Battle-tested physics engine used in games and simulation
- **OpenCL GPU solver**: Parallel constraint solving on GPU for performance at scale
- **Soft body nodes**: Particles connected by distance constraints (links)
- **Material properties**: Configurable stiffness, damping, friction per microbe type
- **Collision detection**: Bullet's broad-phase + narrow-phase with soft contact response

### Microbe Body Plan System

Each microbe type has a unique **body plan** defining its physical structure:

```cpp
struct MicrobeBodyPlan {
    int skeletonCount;              // Internal structural nodes
    int membraneCount;              // Outer elastic nodes
    vector<Vec3> skeletonPositions; // Relative to center
    vector<Vec3> membranePositions; // Outer boundary
    vector<ConstraintDef> constraints; // Links between nodes
    float stiffness;                // Material stiffness
    float damping;                  // Damping factor
};
```

**Example Body Plans**:
- **Amoeba**: 16 skeleton (3 rings) + 16 membrane (circle) = 32 nodes, low stiffness
- **Stentor**: 20 trumpet nodes + 10 contractile stalk nodes, moderate stiffness
- **Lacrymaria**: 16 body + 30 neck chain nodes, very low neck stiffness
- **Diatom**: 8 rigid frustule nodes, infinite stiffness (rigid body)

### Two-Layer Architecture

```
┌─────────────────────────────────────────────────────────────┐
│  PHYSICS LAYER (Bullet Soft Bodies)                         │
│  - Each microbe = btSoftBody with type-specific node count  │
│  - Nodes connected by distance constraints (btLink)         │
│  - Bullet OpenCL solver runs constraint solving on GPU      │
│  - Inter-microbe collisions via Bullet contact detection    │
│  - EC&M forces applied as external forces to nodes          │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼ (sync node positions to SSBOs)
┌─────────────────────────────────────────────────────────────┐
│  RENDER LAYER (Metaball Surface)                            │
│  - Smooth organic surface via metaball field rendering      │
│  - All nodes render as billboards with alpha blending       │
│  - Fragment shader computes metaball field: Σ(r²/d²)        │
│  - Threshold field for surface boundary                     │
│  - Skeleton nodes darker (organelles), membrane lighter     │
│  - Surface deforms naturally with node movement             │
└─────────────────────────────────────────────────────────────┘
```

### Collision Requirements

**REQ-COLLISION-001**: Microbes MUST collide with each other when their visual surfaces overlap, causing visible squishing and deformation.

**REQ-COLLISION-002**: Collision response MUST be soft and elastic, allowing microbes to compress against each other like soft gel bodies.

**REQ-COLLISION-003**: After collision, microbes MUST separate smoothly with a gentle bounce, not stick together or penetrate.

**REQ-COLLISION-004**: Collision radius MUST match the visual size of rendered microbes (metaball surface extent, not just particle centers).

**Implementation**:
- Bullet's btSoftRigidDynamicsWorld handles all collision detection
- Soft body contact parameters tuned for gel-like feel
- Collision margin matches metaball visual extent
- Material friction/restitution for controlled bouncing

### Amoeba Locomotion: EC&M Model (Hybrid Approach)

**Excitable Cortex & Memory (EC&M)** algorithm integrated with Bullet:

1. **12-second cycle**: Extend (0-35%), Search (35-75%), Retract (75-100%)
2. **Pseudopod selection**: One membrane node chosen per cycle
3. **Force application**:
   - Extension: `btSoftBody->addForce(outward_dir * strength, node_idx)`
   - Search: Lateral wiggle (perpendicular force)
   - Retraction: Opposite direction force
4. **Bullet integration**: Forces drive soft body naturally via constraint solver

### Physics Update Pipeline (per frame)

1. **Apply external forces**: EC&M pseudopod forces, cursor attraction, bounds
2. **Step Bullet simulation**: `world->stepSimulation(dt, iterations, dt)`
3. **Sync to SSBOs**: Download node positions from Bullet → CPU buffer → GPU SSBO
4. **Update microbe centers**: Compute center of mass from node positions

### Data Layout (SSBOs for Rendering)

- **Particles**: `vec4 pos, vec4 pos_prev, vec4 vel (xyz + microbe_id), vec4 data (is_membrane flag)`
- **Microbes**: `vec4 center, vec4 color, vec4 params (type, stiffness, seed), vec4 aabb`

SSBOs maintain compatibility with rendering system while physics runs in Bullet.

---

## 8) Rendering & Visuals

### Metaball Rendering System

Microbes render as **organic deformable metaballs** showing internal structure:

**Technique**:
1. **GPU instancing**: One instanced draw call renders all particle billboards
2. **Metaball field**: Fragment shader computes `field = Σ(r²/d²)` for nearby particles
3. **Surface threshold**: Discard fragments below iso-value (e.g., 0.5)
4. **Alpha blending**: Smooth transparency based on field gradient
5. **Internal structure**: Skeleton nodes render darker (organelles), membrane nodes lighter

**Shader Pipeline**:
- **Vertex shader**: Billboard particles in world space (2 triangles per particle)
- **Fragment shader**:
  - Compute distance to particle center
  - Calculate metaball contribution
  - Threshold for surface boundary
  - Modulate alpha by field strength (thick = opaque, thin = transparent)
  - Color variation (skeleton dark, membrane light, pseudopod tips bright)

**Type-Specific Variants**:
- **Soft deformable** (amoeba, stentor): Full metaball blending
- **Spiked** (heliozoa, radiolarian): Thin line rendering for axopodia/spines
- **Rigid** (diatom): Geometric shell with hard edges
- **Ciliated** (stentor): Motion blur or flow lines around cilia

**Advantages**:
- Naturally organic, blob-like appearance
- Shows internal organelles through semi-transparent membrane
- Thin pseudopods render correctly (field varies with deformation)
- Single system handles diverse microbe morphologies
- Deformation reflects physics state in real-time

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
