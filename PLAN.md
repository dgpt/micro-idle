# Micro-Idle Engine - Implementation Plan

## Current Status
- **Phase**: 0 - Clean Slate
- **Last Updated**: 2025-12-28
- **Status**: Architecture redesigned - implementing Puppet/SDF approach
- **Architecture**: Jolt SoftBody physics → SDF Raymarching renderer

---

## Architecture Overview

**New Approach: "The Puppet"**

A clean separation between physics simulation and visual rendering:

1. **Physics Layer (Jolt)**: Low-resolution soft body (icosphere, 32-64 vertices)
   - Point cloud held together by distance constraints
   - Volume constraint for internal pressure
   - Collides with world and other entities
   - Invisible to player (pure physics driver)

2. **The Bridge**: Data transport system
   - Extract vertex positions from Jolt every frame
   - Flatten to shader-compatible format
   - Update GPU uniforms

3. **Rendering Layer (SDF Raymarcher)**: Smooth organic skin
   - GPU fragment shader raymarches through volume
   - SDF smooth union of all vertex positions
   - Renders smooth blob over physics point cloud
   - No mesh drawing - pure procedural rendering

**Tech Stack**:
- **Language**: C++20
- **Build**: CMake with FetchContent
- **Framework**: Raylib 5.5 (windowing, input, rendering)
- **Physics**: Jolt Physics (multi-threaded CPU soft bodies)
- **ECS**: FLECS (Entity Component System)

**Platforms**: Windows (primary), Linux, macOS

---

## Phase 1: Clean Slate

### 1.1 Remove Legacy Code
- [x] Identify all metaball-related files
- [ ] Remove all legacy shaders (metaball_*.vert/frag, xpbd_*.vert/frag)
- [ ] Clean up legacy rendering code in src/
- [ ] Remove game/game.cpp if obsolete
- [ ] Remove any mesh-based rendering systems

### 1.2 Update Documentation
- [x] Update ARCHITECTURE.MD with Puppet/SDF architecture
- [x] Update PLAN.md (this file) with new roadmap
- [ ] Update README.md to remove metaball references

### 1.3 Verify Clean Build
- [ ] Build succeeds with remaining code
- [ ] All existing tests pass
- [ ] No compiler warnings

---

## Phase 2: Foundation - Icosphere Generation

### 2.1 Icosphere Utility
- [ ] Create `src/physics/Icosphere.h/cpp`
- [ ] Implement `GenerateIcosphere(subdivisions)` function
- [ ] Returns vertex positions and triangle indices
- [ ] Target: 32-64 vertices (1-2 subdivisions)
- [ ] Create `tests/test_icosphere.cpp`
- [ ] Verify vertex count, topology correctness

### 2.2 Constraint Generation
- [ ] Create `src/physics/Constraints.h/cpp`
- [ ] Implement `GenerateEdgeConstraints(vertices, triangles)`
- [ ] Generate distance constraints for each edge
- [ ] Implement volume constraint configuration
- [ ] Create `tests/test_constraints.cpp`

---

## Phase 3: Jolt Soft Body Integration

### 3.1 Soft Body Factory
- [ ] Create `src/systems/SoftBodyFactory.h/cpp`
- [ ] Implement `CreateSoftBodyAmoeba(world, position, scale)`
- [ ] Configure `SoftBodyCreationSettings`:
  - Icosphere vertices
  - Edge constraints
  - Volume constraint (internal pressure)
  - Material properties (friction, restitution)
- [ ] Return `JPH::BodyID`

### 3.2 Physics System Update
- [ ] Update `src/systems/PhysicsSystem.h/cpp`
- [ ] Ensure Jolt soft body support is initialized
- [ ] Configure collision filtering (soft body vs world, soft body vs soft body)
- [ ] Test single soft body creation and stepping

### 3.3 Component Definition
- [ ] Update `src/components/Physics.h`
- [ ] Define `SoftBodyComponent`:
  ```cpp
  struct SoftBodyComponent {
      JPH::BodyID bodyID;
      int vertexCount;
      float* vertexPositions;  // Flattened [x,y,z, x,y,z, ...]
  };
  ```

### 3.4 Test Soft Body
- [ ] Create `tests/test_soft_body.cpp`
- [ ] Verify soft body creation
- [ ] Verify vertex extraction works
- [ ] Test collision response
- [ ] Test volume constraint (compression/expansion)

---

## Phase 4: The Bridge - Physics to GPU

### 4.1 Data Extraction System
- [ ] Create `src/systems/UpdateSDFUniforms.h/cpp`
- [ ] Implement `System_UpdateSDFUniforms`:
  - Query entities with `SoftBodyComponent` + `SDFRenderComponent`
  - Lock Jolt BodyInterface
  - Extract vertex positions from soft body
  - Convert `JPH::RVec3` → `float[3]`
  - Flatten to contiguous array
  - Update `SoftBodyComponent.vertexPositions`

### 4.2 Shader Uniform Update
- [ ] In `System_UpdateSDFUniforms`:
  - Get shader location for `uPoints[]`
  - Call `SetShaderValueV(shader, loc, positions, SHADER_UNIFORM_VEC3, count)`
  - Update once per frame

### 4.3 Test Bridge System
- [ ] Create `tests/test_sdf_uniforms.cpp`
- [ ] Mock soft body with known positions
- [ ] Verify data extraction is correct
- [ ] Verify shader uniform format matches expectation

---

## Phase 5: SDF Raymarching Renderer

### 5.1 SDF Shaders
- [ ] Create `data/shaders/sdf_raymarch.vert`
  - Standard MVP transformation
  - Pass world position and view direction to fragment shader
- [ ] Create `data/shaders/sdf_raymarch.frag`
  - Define `uniform vec3 uPoints[64]`
  - Define `uniform int uPointCount`
  - Implement `sdSphere(p, center, radius)`
  - Implement `sdSmoothUnion(d1, d2, k)` (smin)
  - Implement `sceneSDF(p)` (loop over uPoints)
  - Implement raymarch loop
  - Implement normal calculation (SDF gradient)
  - Implement basic Blinn-Phong lighting
- [ ] Test shader compilation

### 5.2 Bounding Volume Generator
- [ ] Create `src/rendering/RaymarchBounds.h/cpp`
- [ ] Implement `GenerateBoundingCube(center, size)`
- [ ] Returns Raylib `Model` (inverted cube mesh)
- [ ] Scale to encompass soft body extents

### 5.3 SDF Render System
- [ ] Create `src/systems/SDFRenderSystem.h/cpp`
- [ ] Implement `System_SDFRender`:
  - Query entities with `Transform` + `SoftBodyComponent` + `SDFRenderComponent`
  - Set shader uniforms (already updated by UpdateSDFUniforms)
  - Bind shader
  - Draw bounding volume
  - Unbind shader

### 5.4 Component Definition
- [ ] Update `src/components/Rendering.h`
- [ ] Define `SDFRenderComponent`:
  ```cpp
  struct SDFRenderComponent {
      Shader sdfShader;
      int shaderLocPoints;
      int shaderLocPointCount;
      int shaderLocCameraPos;
      Model boundingVolume;
  };
  ```

### 5.5 Test Rendering
- [ ] Create `tests/test_sdf_render.cpp`
- [ ] Headless render test (screenshot verification)
- [ ] Verify bounding volume is drawn
- [ ] Verify shader uniforms are correctly bound
- [ ] Visual test: single static soft body renders as smooth blob

---

## Phase 6: Integration - Full Pipeline

### 6.1 Entity Creation Helper
- [ ] Create `src/World.cpp/h` helper function:
  - `CreateAmoeba(world, position, color, seed)`
  - Spawns entity with all required components
  - Creates Jolt soft body
  - Loads SDF shader
  - Generates bounding volume
  - Returns entity ID

### 6.2 Main Loop Integration
- [ ] Update `bin/main.cpp`:
  - Initialize FLECS world
  - Initialize PhysicsSystem (Jolt)
  - Register all systems in correct pipeline order:
    1. Input (OnUpdate)
    2. Physics (OnUpdate)
    3. Transform Sync (OnStore)
    4. SDF Uniform Update (OnStore)
    5. Render (PostUpdate)
  - Spawn test amoeba
  - Run main loop

### 6.3 Visual Verification
- [ ] Create `tests/visual_test.cpp`
- [ ] Spawn single amoeba
- [ ] Let physics run for 60 frames
- [ ] Capture screenshot
- [ ] Verify blob is visible and smooth
- [ ] Verify soft body deformation (if forces applied)

---

## Phase 7: EC&M Locomotion

### 7.1 EC&M Behavior System
- [ ] Create `src/systems/ECMBehaviorSystem.h/cpp`
- [ ] Implement `System_AmoebaBehavior`:
  - Query entities with `ECMLocomotion` + `SoftBodyComponent`
  - Update phase: `ecm.phase += dt / CYCLE_DURATION`
  - State machine:
    - **Extend** (0.0 - 0.4): Apply outward force to target vertex
    - **Search** (0.4 - 0.7): Apply lateral wiggle forces
    - **Retract** (0.7 - 1.0): Apply inward force to all vertices
  - Pseudopod selection: choose vertex on leading edge
  - Apply forces via `BodyInterface.AddForce(bodyID, vertexIndex, force)`

### 7.2 Component Definition
- [ ] Update `src/components/Microbe.h`
- [ ] Define `ECMLocomotion`:
  ```cpp
  struct ECMLocomotion {
      float phase;              // 0.0 - 1.0
      int pseudopodTarget;      // Vertex index
      Vec3 pseudopodDir;        // Extension direction
      float cycleTime;          // 12 seconds default
  };
  ```

### 7.3 Test EC&M
- [ ] Create `tests/test_ecm_locomotion.cpp`
- [ ] Verify phase progression
- [ ] Verify force application at correct vertices
- [ ] Visual test: amoeba extends pseudopod, wiggles, retracts
- [ ] Verify net displacement over multiple cycles

---

## Phase 8: Collision & Multi-Entity

### 8.1 Collision Configuration
- [ ] Configure Jolt collision layers:
  - Soft body vertices collide with ground
  - Soft body vertices collide with other soft body vertices
  - Test collision filtering

### 8.2 Ground Plane
- [ ] Create static ground plane (Jolt box body)
- [ ] Verify soft bodies rest on ground
- [ ] Verify soft deformation on impact

### 8.3 Multi-Entity Test
- [ ] Spawn 2-3 amoebas in proximity
- [ ] Verify inter-amoeba collision
- [ ] Verify soft squishing behavior
- [ ] Verify separation after collision
- [ ] Visual test: amoebas collide and deform realistically

---

## Phase 9: Polish & Optimization

### 9.1 Shader Optimization
- [ ] Profile fragment shader performance
- [ ] Optimize raymarch step count
- [ ] Implement early ray termination
- [ ] Add bounding sphere culling

### 9.2 Camera System
- [ ] Implement simple camera controller
- [ ] Orbit camera around scene
- [ ] Zoom in/out
- [ ] Lock to 2D plane (top-down view)

### 9.3 Visual Polish
- [ ] Tune SDF smoothness parameter
- [ ] Tune vertex radius for desired blob appearance
- [ ] Add color variation per microbe
- [ ] Test different lighting models (ambient, diffuse, specular)

### 9.4 Performance Testing
- [ ] Spawn 10+ amoebas
- [ ] Profile frame time
- [ ] Profile physics time
- [ ] Profile render time
- [ ] Identify bottlenecks
- [ ] Optimize as needed

---

## Phase 10: Future Extensions

### 10.1 Additional Microbe Types
- [ ] Stentor (elongated soft body)
- [ ] Heliozoa (spikes via separate rigid bodies)
- [ ] Different icosphere resolutions per type

### 10.2 Rendering Variations
- [ ] Type-specific shader variants
- [ ] Internal structure rendering (skeleton nodes)
- [ ] Transparency/alpha blending

### 10.3 Gameplay Systems
- [ ] Resource drops on destruction
- [ ] Player interaction (click to destroy)
- [ ] Spawning system

---

## Key Files

**Docs**: README.md, ARCHITECTURE.MD, PLAN.md (this file)
**Build**: CMakeLists.txt, bin/build.sh
**Components**: src/components/*.h
**Systems**: src/systems/*.cpp
**Shaders**: data/shaders/sdf_raymarch.*
**Main**: bin/main.cpp

---

## Cleanup Checklist

Files to DELETE:
- [x] Identified legacy shader files (metaball_*, xpbd_*)
- [ ] `data/shaders/metaball.vert/frag`
- [ ] `data/shaders/metaball_field.vert/frag`
- [ ] `data/shaders/metaball_surface.vert/frag`
- [ ] `data/shaders/xpbd_microbe.vert/frag`
- [ ] `data/shaders/particle_simple.vert/frag`
- [ ] `data/shaders/outline.vert/frag`
- [ ] `data/shaders/outline_curve.vert/frag`
- [ ] Any obsolete mesh rendering code in src/
- [ ] Any obsolete rendering components/systems

Code to REFACTOR:
- [ ] Keep PhysicsSystem.cpp (update for soft bodies)
- [ ] Keep SoftBodyFactory.cpp (update for new approach)
- [ ] Keep ECMLocomotionSystem.cpp (update to apply forces to vertices)
- [ ] Keep World.cpp (update entity creation)
- [ ] Remove any metaball/mesh rendering code

---

## Next Immediate Steps

1. Remove all legacy shader files
2. Update README.md
3. Verify clean build
4. Begin Phase 2: Icosphere generation
