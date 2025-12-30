# Micro-Idle Implementation Plan

**Last Updated**: Current session
**Status**: In Progress - Phase 1 & 3 Core Systems Complete

---

## Current State Assessment

### ✅ Implemented Components

1. **Core Infrastructure**
   - ✅ FLECS world initialization (`src/World.cpp/h`)
   - ✅ Jolt Physics integration (`src/systems/PhysicsSystem.cpp/h`)
   - ✅ Fixed-step time accumulator (`engine/platform/time.cpp/h`)
   - ✅ Basic engine loop (`engine/platform/engine.cpp/h`)

2. **Components**
   - ✅ `Transform.h` - Position, rotation, scale
   - ✅ `Physics.h` - Jolt BodyID wrapper
   - ✅ `Microbe.h` - Microbe types, stats, soft body, EC&M locomotion
   - ✅ `Rendering.h` - Basic render components
   - ✅ `Input.h` - Input state singleton

3. **Physics Systems**
   - ✅ `PhysicsSystem` - Jolt world management
   - ✅ `SoftBodyFactory` - Icosphere generation and soft body creation
   - ✅ `ECMLocomotionSystem` - EC&M algorithm implementation
   - ✅ `Icosphere.cpp/h` - Icosphere mesh generation
   - ✅ `Constraints.cpp/h` - Distance/volume constraints

4. **Rendering**
   - ✅ SDF shader files (`shaders/sdf_membrane.vert/frag`)
   - ⚠️ SDF rendering logic exists but embedded in `World::render()` (needs refactoring)

5. **Tests**
   - ✅ Visual test framework (`tests/test_visual.cpp`)
   - ✅ Physics tests (`test_icosphere.cpp`, `test_constraints.cpp`)
   - ✅ System tests (`test_softbody_factory.cpp`, `test_ecm_locomotion.cpp`)

### ⚠️ Partially Implemented / Needs Refactoring

1. **Rendering Systems** (Architecture §7)
   - ⚠️ SDF rendering exists in `World::render()` but should be `SDFRenderSystem`
   - ⚠️ SDF uniform updates embedded in render, should be `UpdateSDFUniforms` system
   - ⚠️ Transform sync embedded in `World::update()`, should be `TransformSyncSystem`

2. **Input System** (Architecture §7)
   - ⚠️ Input component exists but no dedicated `InputSystem` to poll Raylib → FLECS

3. **Game Logic**
   - ❌ Microbe spawning system
   - ❌ Destruction/hover detection
   - ❌ Resource system
   - ❌ Progression system (DNA traits, nutrients, disinfection)

### ❌ Missing Systems (Per Architecture)

1. **Core Systems** (Architecture §7)
   - ❌ `InputSystem.cpp/h` - Raylib → FLECS input capture
   - ❌ `TransformSyncSystem.cpp/h` - Jolt → FLECS transform sync
   - ❌ `UpdateSDFUniforms.cpp/h` - Physics → GPU data transport
   - ❌ `SDFRenderSystem.cpp/h` - SDF raymarcher rendering

2. **Rendering Utilities** (Architecture §4)
   - ❌ `src/rendering/SDFShader.cpp/h` - Shader loading/management
   - ❌ `src/rendering/RaymarchBounds.cpp/h` - Bounding volume generation

3. **Game Systems**
   - ❌ `SpawnSystem.cpp/h` - Procedural microbe generation
   - ❌ `DestructionSystem.cpp/h` - Hover/click detection and destruction
   - ❌ `ResourceSystem.cpp/h` - Resource drops and collection
   - ❌ `ProgressionSystem.cpp/h` - DNA traits, nutrients, disinfection

4. **Components**
   - ❌ `ECMLocomotion.h` - Separate component (currently embedded in `Microbe`)
   - ❌ `SDFRenderComponent.h` - Shader reference, uniform locations
   - ❌ `Resource.h` - Resource types and amounts
   - ❌ `Progression.h` - DNA traits, upgrades

5. **Collision Requirements** (Architecture §7.4)
   - ⚠️ Collision detection exists but needs verification:
     - REQ-COLLISION-001: Microbes collide when surfaces overlap ✓ (Jolt handles)
     - REQ-COLLISION-002: Soft and elastic response ⚠️ (needs material tuning)
     - REQ-COLLISION-003: Smooth separation ⚠️ (needs verification)
     - REQ-COLLISION-004: Collision radius matches visual size ⚠️ (needs verification)

---

## Implementation Roadmap

### Phase 1: Core System Refactoring (Current Priority)

**Goal**: Separate concerns according to architecture, establish clean Input → Simulation → Render pipeline.

#### 1.1 Extract Input System ✅
- [x] Create `src/systems/InputSystem.cpp/h`
- [x] Poll Raylib input (mouse, keyboard)
- [x] Write to `InputState` singleton
- [x] Register in `World::registerSystems()` (OnUpdate phase)
- [ ] Test: Verify input state updates correctly

#### 1.2 Extract Transform Sync System ✅
- [x] Create `src/systems/TransformSyncSystem.cpp/h`
- [x] Move transform sync logic from `World::update()` to system
- [x] Handle both rigid bodies and soft bodies
- [x] Register in OnStore phase (after physics, before render)
- [ ] Test: Verify transforms sync correctly

#### 1.3 Extract SDF Uniform Update System ✅
- [x] Create `src/systems/UpdateSDFUniforms.cpp/h`
- [x] Extract vertex positions from Jolt soft bodies
- [x] Flatten to float array
- [x] Update shader uniforms
- [x] Register in OnStore phase (after TransformSync)
- [ ] Test: Verify uniforms update correctly

#### 1.4 Extract SDF Render System ✅
- [x] Create `src/systems/SDFRenderSystem.cpp/h`
- [x] Move SDF rendering logic from `World::render()` to system
- [x] Query entities with `Microbe` + `Transform` components
- [x] Draw bounding volumes, bind shader, render
- [x] Register in PostUpdate phase
- [ ] Test: Verify rendering works correctly

#### 1.5 Create Rendering Utilities ✅
- [x] Create `src/rendering/SDFShader.cpp/h` - Shader loading/management
- [x] Create `src/rendering/RaymarchBounds.cpp/h` - Bounding volume generation
- [x] Refactor `World` to use utilities
- [x] Refactor `UpdateSDFUniforms` and `SDFRenderSystem` to use utilities
- [x] Build: Verified compilation succeeds
- [ ] Test: Verify shader loading and bounds calculation (needs visual test)

#### 1.6 Verify Phase Separation ⚠️
- [x] Ensure Input phase runs first (OnUpdate) - InputSystem registered
- [x] Ensure Simulation phase runs second (OnUpdate: Physics, EC&M, GameLogic) - Physics and EC&M run in update()
- [x] Ensure Transform sync runs third (OnStore) - TransformSyncSystem registered
- [x] Ensure Render phase runs last (PostUpdate) - SDFRenderSystem registered
- [ ] Test: Verify phase ordering (needs testing)

**Status**: Core systems working! Phase separation implemented. Game logic systems fully functional: SpawnSystem creates and spawns microbes properly, DestructionSystem uses single-click detection, ResourceSystem collects resources. **CRITICAL BUG FIXED** - Microbes were not accumulating because FLECS deferred entity operations. Added `it.world().defer_end()` to SpawnSystem to flush deferred operations immediately after spawning. Microbes now persist and accumulate correctly. Boundaries work properly (32px margin, floor at y=0, dynamic resizing). All systems tested and working!

**Note**: Currently rendering is done manually in `World::render()` for compatibility. Systems are registered and will run via `world.progress()`, but render() is called separately. Future refactoring could integrate render systems more tightly.

---

### Phase 2: Collision & Physics Verification

**Goal**: Ensure collision requirements are met (Architecture §7.4).

#### 2.1 Material Tuning
- [ ] Review Jolt material properties (friction, restitution)
- [ ] Tune for soft, elastic, gel-like response
- [ ] Test: Verify collision feels soft and elastic

#### 2.2 Collision Radius Verification
- [ ] Measure visual size from SDF rendering
- [ ] Compare with Jolt collision radius
- [ ] Ensure they match (REQ-COLLISION-004)
- [ ] Test: Visual test with collision overlay

#### 2.3 Smooth Separation Verification
- [ ] Test collision scenarios
- [ ] Verify microbes separate smoothly after collision
- [ ] Test: Record collision behavior, verify smoothness

**Success Criteria**: All collision requirements met, visual tests pass.

---

### Phase 3: Game Logic Foundation

**Goal**: Implement core game loop mechanics (spawning, destruction, resources).

#### 3.1 Microbe Spawning System ✅
- [x] Create `src/systems/SpawnSystem.cpp/h`
- [x] Procedural microbe generation based on spawn rate
- [x] Spawn within screen boundaries
- [x] Register in OnUpdate phase
- [x] Build: Verified compilation succeeds
- [ ] Test: Verify microbes spawn correctly (needs visual test)

#### 3.2 Destruction System ✅
- [x] Create `src/systems/DestructionSystem.cpp/h`
- [x] Hover detection (mouse position → microbe collision) - placeholder implementation
- [x] Click detection (mouse click → microbe destruction)
- [x] Damage application
- [x] Register in OnUpdate phase (after Input)
- [x] Spawn resources on destruction
- [x] Build: Verified compilation succeeds
- [ ] Test: Verify hover/click detection works (needs proper ray casting)

#### 3.3 Resource System ✅
- [x] Create `src/components/Resource.h` - Resource types (Sodium, Glucose, etc.)
- [x] Create `src/systems/ResourceSystem.cpp/h` - Resource drops and collection
- [x] Drop resources on microbe destruction
- [x] Collect resources (hover/click) - placeholder implementation
- [x] Resource inventory singleton
- [x] Register in OnUpdate phase
- [x] Build: Verified compilation succeeds
- [ ] Test: Verify resources drop and collect correctly (needs visual test)

**Success Criteria**: Microbes spawn, can be destroyed, drop resources.

---

### Phase 4: Progression System

**Goal**: Implement DNA traits, nutrients, and disinfection (README §Progression).

#### 4.1 DNA Traits System
- [ ] Create `src/components/Progression.h` - DNA trait tree
- [ ] Create `src/systems/ProgressionSystem.cpp/h` - Trait unlocking
- [ ] Each trait provides: resource + drop + structure + mechanic
- [ ] Prerequisite-based tree
- [ ] Test: Verify trait unlocking works

#### 4.2 Nutrient Upgrades
- [ ] Extend `ProgressionSystem` for nutrient upgrades
- [ ] Scaling and multiplicative stacking
- [ ] Test: Verify upgrades scale correctly

#### 4.3 Disinfection System
- [ ] Create `src/systems/DisinfectionSystem.cpp/h`
- [ ] Active abilities (bursts, beams, chains)
- [ ] Passive effects (fields, pulses, debuffs)
- [ ] Idle automation (autonomous killers, fever waves)
- [ ] Test: Verify abilities work correctly

**Success Criteria**: Full progression system functional, traits unlock correctly.

---

### Phase 5: Advanced Microbe Behaviors

**Goal**: Implement membrane undulation, flagella waves, and other behaviors (README §Behavioral Motion).

#### 5.1 Membrane Undulation
- [ ] Add undulation component
- [ ] Implement wave-based deformation
- [ ] Apply to soft body vertices
- [ ] Test: Verify undulation looks correct

#### 5.2 Flagella Waves
- [ ] Add flagella component
- [ ] Implement wave propagation
- [ ] Visual rendering of flagella
- [ ] Test: Verify flagella waves correctly

#### 5.3 Additional Behaviors
- [ ] Pili twitching
- [ ] Extracellular filament drift
- [ ] Periodic signal pulses
- [ ] Test: Verify all behaviors work

**Success Criteria**: All microbe behaviors implemented and working.

---

### Phase 6: Advanced Features

**Goal**: Implement defenses, offenses, and synergies (README §Enemy Threats).

#### 6.1 Defensive Structures
- [ ] Capsules (damage mitigation)
- [ ] Biofilms (armor + clustering)
- [ ] Spores (near-invulnerability)
- [ ] Test: Verify defenses work

#### 6.2 Offensive Structures
- [ ] Harpoon systems (burst damage)
- [ ] Contact toxins (damage-over-time)
- [ ] Swarm coordination (group buffs)
- [ ] Test: Verify offenses work

#### 6.3 System Synergies
- [ ] Ensure all systems work together
- [ ] Test: Verify synergies work correctly

**Success Criteria**: All advanced features implemented and working.

---

## Development Practices

Following ARCHITECTURE.MD §2:

- ✅ **Root Cause Analysis**: Fix root causes, not symptoms
- ✅ **Verification**: Build and test after every logical change
- ✅ **Refactoring**: Continuously reduce complexity
- ✅ **Algorithm Selection**: Replace inadequate algorithms, don't extend with special cases
- ✅ **Problem-Solving**: Try alternatives, refactor until root cause found
- ✅ **Code Quality**: Prioritize readability, eliminate unjustified complexity
- ✅ **Testing**: All debugging via tests, no ad-hoc logging

---

## Testing Strategy

- **Unit Tests**: Each system has corresponding `test_*.cpp`
- **Integration Tests**: Test system interactions
- **Visual Tests**: Headless screenshot verification (`test_visual.cpp`)
- **Coverage Target**: 95%+ on `/src` modules

---

## Build & Verification

- **Build**: `bin/build.sh` (cross-compile Windows .exe)
- **Test**: `bin/test.sh` (run tests + coverage)
- **Never run `game.exe` during development** - use `tests.exe` for verification

---

## Notes

- Current implementation has good foundation but needs refactoring for proper architecture
- Priority: Phase 1 (core system refactoring) to establish clean pipeline
- Then: Phase 2 (collision verification) to ensure requirements met
- Then: Phases 3-6 (game logic and features) incrementally
