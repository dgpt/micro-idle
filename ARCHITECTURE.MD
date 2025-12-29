# Micro-Idle — Architecture

## 1) Goals

- Code-first, minimal dependencies, single-binary runtime
- CPU multi-threaded physics (Jolt), GPU rendering (Raylib)
- **Primary target: Windows .exe** (cross-compiled from WSL for GPU access)
- Cross-platform support: Linux, macOS, Android, iOS (future)
- ECS architecture (FLECS) for clean entity/component separation
- Stable frame pacing and predictable performance at scale
- Determinism-friendly time model (fixed step)
- Small, composable, testable modules

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

- **Language:** C++20
- **Framework:** Raylib 5.5 (window/input/audio/rendering)
- **ECS:** FLECS (Entity Component System)
- **Physics:** Jolt Physics (multi-threaded CPU soft body simulation)
- **Graphics:** OpenGL 4.6 / Metal / Vulkan (platform-dependent, via Raylib)
  - Linux: OpenGL 4.6 via Mesa
  - macOS: Metal via Raylib backend
  - Windows: OpenGL 4.6 / DirectX via Raylib
- **Multi-threaded:** Jolt physics uses all CPU cores, FLECS systems run in pipeline phases

---

## 4) File System Structure (Lean & Modular)

### Principles
- **One concern per file**: Each file has a single, clear responsibility
- **Small & composable**: Prefer many small files over few large files
- **Test co-location**: Each `.cpp` file has a corresponding `test_*.cpp`
- **Pure functions**: Stateless functions in separate files when possible

### Directory Structure

```
/src                          # All game code (FLECS/Jolt based)
  /components                 # FLECS component definitions (header-only)
    Transform.h               # Position, rotation, scale
    Physics.h                 # Jolt BodyID, soft body reference
    Microbe.h                 # MicrobeType, seed, stats
    ECMLocomotion.h           # EC&M state machine data
    Rendering.h               # Shader reference, SDF parameters
    Input.h                   # Input state component

  /systems                    # FLECS systems (one system per file)
    PhysicsSystem.cpp/h       # Jolt initialization, world stepping
    SoftBodyFactory.cpp/h     # Create Jolt soft bodies (icosphere)
    TransformSyncSystem.cpp/h # Jolt → FLECS transform sync
    ECMBehaviorSystem.cpp/h   # EC&M algorithm implementation
    SDFRenderSystem.cpp/h     # SDF raymarcher rendering
    UpdateSDFUniforms.cpp/h   # Bridge: Physics → GPU data transport
    InputSystem.cpp/h         # Raylib → FLECS input capture

  /physics                    # Jolt physics utilities (pure functions)
    SoftBodyConfig.cpp/h      # Soft body creation settings
    Constraints.cpp/h         # Distance/volume constraints
    Materials.cpp/h           # Friction, restitution presets
    Icosphere.cpp/h           # Icosphere mesh generation

  /rendering                  # Rendering utilities (pure functions)
    SDFShader.cpp/h           # SDF shader loading and management
    RaymarchBounds.cpp/h      # Bounding volume generation
    ShaderLoader.cpp/h        # Load and compile shaders

  /math                       # Math utilities (header-only, pure)
    Vec3.h                    # 3D vector operations
    Quat.h                    # Quaternion operations
    Random.h                  # RNG utilities

  World.cpp/h                 # FLECS world wrapper and initialization

/engine                       # Platform abstraction (minimal)
  /platform
    engine.cpp/h              # Window, context, main loop
    time.cpp/h                # Fixed-step accumulator
  /util
    rng.cpp/h                 # Random number generation
    logging.h                 # Logging macros (header-only)

/tests                        # Tests mirror /src structure
  /components                 # Component tests
    test_Transform.cpp
    test_Physics.cpp
  /systems                    # System tests
    test_PhysicsSystem.cpp
    test_SoftBodyFactory.cpp
    test_ECMBehaviorSystem.cpp
  /physics                    # Physics utility tests
    test_SoftBodyConfig.cpp
    test_Icosphere.cpp
  /rendering                  # Rendering utility tests
    test_SDFShader.cpp

  test_main.cpp               # Test runner
  test_time.cpp               # Platform tests
  test_rng.cpp
  test_engine.cpp

/data
  /shaders                    # GLSL shaders
    sdf_raymarch.vert         # SDF raymarcher vertex shader
    sdf_raymarch.frag         # SDF raymarcher fragment shader
  /raw                        # Source assets (future)

/bin                          # Build scripts
  build.sh                    # Configure + build
  clean.sh                    # Clean cache
  test.sh                     # Run tests + coverage

CMakeLists.txt
README.md                     # Game design document
ARCHITECTURE.MD               # This file - code architecture
PLAN.md                       # Implementation roadmap
```

### File Naming Conventions

**Components** (header-only):
- `ComponentName.h` - FLECS component struct definition
- Example: `Transform.h`, `Microbe.h`

**Systems** (implementation + header):
- `SystemNameSystem.cpp/h` - FLECS system implementation
- Example: `PhysicsSystem.cpp`, `SDFRenderSystem.cpp`

**Utilities** (pure functions):
- `ModuleName.cpp/h` - Stateless utility functions
- Example: `SoftBodyConfig.cpp`, `Icosphere.cpp`

**Tests**:
- `test_ModuleName.cpp` - Tests for corresponding module
- Example: `test_PhysicsSystem.cpp`

---

## 5) Build & Targets

**CMake targets:**
- `game.exe` - Main executable (Windows)
- `tests.exe` - Test suite with headless screenshot output (Windows)

**Build requirements:**
- C++20 standard (required for FLECS and Jolt)
- CMake 3.15+
- MinGW-w64 cross-compiler (x86_64-w64-mingw32-gcc/g++)
- OpenGL 4.6+ support (via Raylib, uses Windows GPU)

**Development environment:**
- WSL2 for building (cross-compilation to Windows)
- Windows GPU for execution

**Quality gates:**
- All tests must pass before merge
- Tests run headlessly and output screenshots for verification
- **NEVER run game.exe during development** (opens disruptive window)
- **ALL verification via test.exe** (headless screenshot output)
- Coverage target: **95%+** on `/src` modules
- Zero compiler warnings on MinGW

**Build scripts:**
- `bin/build.sh` - Cross-compile Windows .exe files
- `bin/build.sh --clean` - Clean rebuild

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

## 7) The Simulation Loop (FLECS + Jolt)

The architecture follows a **three-phase pipeline** that decouples simulation from rendering:

```
┌─────────────────────────────────────────────────────────────┐
│  INPUT PHASE (Raylib → FLECS)                               │
│  - Poll Raylib input (mouse, keyboard, touch)               │
│  - Write to FLECS InputComponent                            │
│  - Single source of truth for input state                   │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│  SIMULATION PHASE (FLECS Systems)                           │
│                                                              │
│  System_UpdatePhysics:                                      │
│    - Step Jolt physics world (multi-threaded)              │
│    - Apply EC&M forces to soft body vertices               │
│    - Handle collisions and constraints                      │
│                                                              │
│  System_SyncTransforms:                                     │
│    - Read Jolt soft body positions                          │
│    - Write to FLECS Transform components                    │
│                                                              │
│  System_UpdateSDFUniforms:                                  │
│    - Extract soft body vertex positions                     │
│    - Flatten to float array                                 │
│    - Update shader uniforms                                 │
│                                                              │
│  System_GameLogic:                                          │
│    - Query FLECS entities (microbes, resources, etc.)       │
│    - Update game state (health, resources, behaviors)       │
│    - Spawn/destroy entities as needed                       │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│  RENDER PHASE (FLECS → Raylib)                              │
│                                                              │
│  System_SDFRender:                                          │
│    - Query entities with SDF rendering components           │
│    - Draw bounding volume (inverted cube)                   │
│    - Bind SDF shader with vertex position uniforms          │
│    - Raymarcher computes smooth skin over point cloud       │
│    - Use camera from FLECS singleton                        │
└─────────────────────────────────────────────────────────────┘
```

### FLECS Component Architecture

**Core Components**:
```cpp
struct Transform {
    Vec3 position;
    Quat rotation;
    Vec3 scale;
};

struct SoftBodyComponent {
    JPH::BodyID bodyID;        // Jolt soft body handle
    int vertexCount;           // Number of vertices (32-64)
    float* vertexPositions;    // Flattened world space positions
};

struct Microbe {
    MicrobeType type;
    float seed;                // Procedural variation
    Color color;
};

struct ECMLocomotion {
    float phase;               // 0-1 cycle position
    int pseudopodTarget;       // Which vertex is extending
    Vec3 pseudopodDir;         // Extension direction
};

struct SDFRenderComponent {
    Shader sdfShader;
    int shaderLocPoints;       // Uniform location for vertex positions
    Model boundingVolume;      // Cube for raymarching bounds
};
```

**FLECS Pipeline Phases**:
- `OnUpdate`: Physics, EC&M behavior, game logic
- `OnStore`: Transform synchronization, SDF uniform updates
- `PostUpdate`: Rendering

### Jolt Physics Integration

**Initialization**:
```cpp
// In PhysicsSystem::init()
JPH::PhysicsSystem physicsSystem;
physicsSystem.Init(maxBodies, numBodyMutexes, maxBodyPairs, maxContactConstraints,
                   broadPhaseLayer, objectVsBroadPhase, objectVsObject);
```

**The Puppet Architecture (Physics → Rendering Bridge)**:

1. **Physics Layer**: Jolt Soft Body
   - Low-resolution icosphere (32-64 vertices)
   - Distance constraints (edges) hold structure together
   - Volume constraint maintains internal pressure
   - Vertices collide with world and other entities
   - Invisible to player (pure physics driver)

2. **The Bridge**: `System_UpdateSDFUniforms`
   - Locks Jolt BodyInterface
   - Extracts current world space vertex positions
   - Flattens to `float positions[MAX_VERTS * 3]`
   - Updates shader uniform every frame

3. **Rendering Layer**: SDF Raymarcher
   - Draws bounding cube (not the soft body mesh itself)
   - Fragment shader raymarches through volume
   - For each ray step: compute SDF as smooth union of all vertex positions
   - Renders smooth organic skin over the point cloud
   - Normal calculated from SDF gradient

**Soft Body Configuration**:
```cpp
SoftBodyCreationSettings settings;
settings.mVertices = GenerateIcosphere(subdivisions);  // 32-64 verts
settings.mEdgeConstraints = GenerateEdgeConstraints(mesh);
settings.mVolumeConstraints = { VolumeConstraint(...) };  // Internal pressure
```

**Multi-threading**:
- Jolt job system uses all CPU cores
- FLECS systems can run in parallel on independent queries
- Physics substeps run in background while other systems prepare next frame

### Collision Requirements

**REQ-COLLISION-001**: Microbes MUST collide when surfaces overlap, causing visible deformation.

**REQ-COLLISION-002**: Collision response MUST be soft and elastic (gel-like).

**REQ-COLLISION-003**: After collision, microbes MUST separate smoothly.

**REQ-COLLISION-004**: Collision radius MUST match visual size.

**Implementation**:
- Jolt's contact listener for collision events
- Material properties (friction, restitution) tuned for soft response
- Soft body vertices deform on collision
- SDF renderer shows deformation in real-time

### EC&M Locomotion (FLECS System)

**System_AmoebaBehavior** (runs in OnUpdate phase):
```cpp
void System_AmoebaBehavior(flecs::entity e, Transform& t, SoftBodyComponent& sb, ECMLocomotion& ecm) {
    // Update cycle phase
    ecm.phase += dt / CYCLE_DURATION;

    // Apply forces to specific vertices via Jolt
    if (extend_phase) {
        int targetVertex = ecm.pseudopodTarget;
        jolt->BodyInterface.AddForce(sb.bodyID, targetVertex, pseudopod_force);
    } else if (search_phase) {
        jolt->BodyInterface.AddForce(sb.bodyID, targetVertex, wiggle_force);
    } else if (retract_phase) {
        jolt->BodyInterface.AddForce(sb.bodyID, targetVertex, retract_force);
    }
}
```

---

## 8) Rendering & Visuals

### SDF Raymarching System

Microbes render as **smooth organic blobs** via GPU raymarching over the physics point cloud:

**Technique**:
1. **Bounding Volume**: Draw inverted cube scaled to encompass soft body
2. **Raymarch Loop**: Fragment shader marches ray through volume
3. **SDF Function**: `sdSmoothUnion` (smin) of all vertex positions
4. **Surface Detection**: When distance < threshold, render surface
5. **Lighting**: Blinn-Phong based on SDF gradient (surface normal)

**Shader Pipeline**:
- **Vertex shader** (`sdf_raymarch.vert`):
  - Standard MVP transformation
  - Pass ray origin and direction to fragment shader

- **Fragment shader** (`sdf_raymarch.frag`):
  ```glsl
  uniform vec3 uPoints[64];  // Soft body vertex positions
  uniform int uPointCount;
  uniform vec3 uCameraPos;

  float sdSphere(vec3 p, vec3 center, float radius) {
      return length(p - center) - radius;
  }

  float sdSmoothUnion(float d1, float d2, float k) {
      float h = clamp(0.5 + 0.5 * (d2 - d1) / k, 0.0, 1.0);
      return mix(d2, d1, h) - k * h * (1.0 - h);
  }

  float sceneSDF(vec3 p) {
      float d = sdSphere(p, uPoints[0], VERTEX_RADIUS);
      for (int i = 1; i < uPointCount; i++) {
          d = sdSmoothUnion(d, sdSphere(p, uPoints[i], VERTEX_RADIUS), SMOOTHNESS);
      }
      return d;
  }

  void main() {
      // Raymarch from camera through fragment
      vec3 rayDir = normalize(fragWorldPos - uCameraPos);
      vec3 rayPos = fragWorldPos;

      for (int step = 0; step < MAX_STEPS; step++) {
          float dist = sceneSDF(rayPos);
          if (dist < EPSILON) {
              // Hit surface - calculate normal and light
              vec3 normal = calcNormal(rayPos);
              vec3 color = blinnPhong(rayPos, normal, uCameraPos);
              FragColor = vec4(color, 1.0);
              return;
          }
          rayPos += rayDir * dist;
      }

      discard;  // No hit
  }
  ```

**Advantages**:
- Naturally organic, blob-like appearance
- Smooth deformation reflects physics state in real-time
- No mesh rendering artifacts
- Single system handles diverse microbe morphologies
- GPU-accelerated raymarching is highly efficient
- Physics can use low vertex count (32-64) while rendering is smooth

**Performance Considerations**:
- Keep soft body vertex count low (32-64 max)
- Use early ray termination
- Limit raymarch steps (64-128 typical)
- Bounding volume culling
- LOD via vertex count reduction at distance

---

## 9) Testing Strategy

**Test Organization:**
```
/tests
  test_main.cpp           # Test runner
  test_time.cpp           # Platform layer tests
  test_rng.cpp            # RNG tests
  test_engine.cpp         # Engine init tests
  test_game_constants.cpp # Constants validation

  # Physics tests:
  test_soft_body.cpp      # Jolt soft body creation
  test_icosphere.cpp      # Icosphere generation
  test_constraints.cpp    # Distance/volume constraints

  # Rendering tests:
  test_sdf_shader.cpp     # SDF shader compilation
  test_raymarch.cpp       # Raymarching logic

  # System tests:
  test_physics_system.cpp # Jolt integration tests
  test_ecm_locomotion.cpp # EC&M algorithm tests
  test_sdf_uniforms.cpp   # Bridge system tests
```

**Testing Principles:**
- Every component has a test
- Every system has integration tests
- Tests are small, fast, focused (one assertion per test preferred)
- Use fixtures for complex setup
- Mock Jolt physics for unit tests when needed

**Coverage Requirements:**
- **95%+** line coverage on `/src/components` and `/src/systems`
- **90%+** branch coverage
- Measure via `bin/check_coverage.sh` (uses gcov/lcov)

**Test Naming Convention:**
- `test_<module>_<function>_<scenario>`
- Example: `test_icosphere_generate_hasCorrectVertexCount`

---

## 10) Data & Assets

```
/data
  /raw       # Source assets (PNG, WAV, OBJ, fonts)
  /shaders   # GLSL shader sources (.vert, .frag, .comp)
  /baked     # Processed outputs for release (packed archives, atlases)
```

**Asset Pipeline:**
- Assets loaded from `data/raw` in dev mode
- Assets packed to `data/baked/assets.pak` for release
- Shaders compiled at runtime (cached)

---

## 11) Future Modules

Modules to be implemented as needed:

- `/src/resources` - Resource management system (sodium, glucose, etc.)
- `/src/progression` - DNA traits and upgrades
- `/src/ui` - Game UI components (separate from engine UI)
- `engine/assets` - Asset loading and caching
- `engine/io` - VFS and serialization
- `engine/debug` - Dev console, profiling overlay
