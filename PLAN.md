# Micro-Idle Implementation Plan

**Last Updated**: December 30, 2025
**Status**: CRITICAL ISSUE - Rendering Pipeline Broken
**Visual Validation**: ❌ FAIL - Shows geometric shapes, not organic SDF microbes

---

## **CRITICAL ISSUE IDENTIFIED**

**Problem**: 3D rendering in World::render() produces black screens, but 2D rendering works in World::renderUI().

**Root Cause**: OpenGL/Raylib context issue - DrawSphere and 3D functions don't render in World::render() context, but DrawCircle and 2D functions work in World::renderUI().

**Evidence**:
- 3D rendering (DrawSphere) in World::render() produces black screenshots
- 2D rendering (DrawCircle) in World::renderUI() produces visible microbe circles
- Microbe data and positions are correct
- Rendering code executes but 3D output is invisible
- UI text renders correctly, proving screenshot capture works

**Impact**: Cannot validate any visual changes with `bin/vlm` until basic rendering works

---

## Current State Assessment (Accurate as of Dec 30, 2025)

### ✅ **Implemented Core Systems** (Architecture Compliant)

1. **FLECS ECS Infrastructure**
   - ✅ FLECS world initialization and management
   - ✅ Component definitions: `Transform.h`, `Physics.h`, `Microbe.h`, `Input.h`, `Rendering.h`
   - ✅ System registration pipeline with proper phase ordering

2. **Physics Pipeline (Jolt Integration)**
   - ✅ `PhysicsSystem` - Complete Jolt world management
   - ✅ `SoftBodyFactory` - Icosphere soft body creation (32-64 vertices)
   - ✅ `ECMLocomotionSystem` - EC&M algorithm for amoeba movement
   - ✅ Physics utilities: `Icosphere.cpp/h`, `Constraints.cpp/h`
   - ✅ Multi-threaded physics simulation

3. **FLECS Systems Architecture**
   - ✅ `InputSystem` - Raylib input capture → FLECS
   - ✅ `TransformSyncSystem` - Jolt → FLECS sync (OnStore phase)
   - ✅ `UpdateSDFUniforms` - Physics vertex extraction → GPU (OnStore phase)
   - ✅ `SDFRenderSystem` - Registered in PostUpdate phase (but bypassed)

4. **Rendering Infrastructure**
   - ✅ SDF shader files: `sdf_membrane.vert/frag`
   - ✅ Rendering utilities: `SDFShader.cpp/h`, `RaymarchBounds.cpp/h`
   - ✅ Shader loading and uniform management

5. **Game Logic Foundation**
   - ✅ `SpawnSystem` - Procedural microbe generation with boundaries
   - ✅ `DestructionSystem` - Click detection framework
   - ✅ `ResourceSystem` - Resource drops and collection
   - ✅ World boundaries with 32px margin, dynamic resizing

6. **Testing Infrastructure**
   - ✅ Visual test framework with screenshot validation
   - ✅ Physics and system integration tests
   - ✅ Headless test execution for development validation

### ❌ **BROKEN: Rendering Pipeline** (Architecture Violation)

**Issue**: World::render() performs manual geometric rendering instead of using SDFRenderSystem.

**Impact**: Produces stylized geometric shapes instead of smooth organic microbes.

**Required Fix**: Remove manual rendering from World::render(), ensure SDFRenderSystem handles all microbe rendering.

### ⚠️ **Needs Verification: Collision Physics**

**Status**: Implemented but unverified against requirements (Architecture §7.4)

- REQ-COLLISION-001: Surface overlap collision ✅ (Jolt handles)
- REQ-COLLISION-002: Soft elastic response ⚠️ (needs tuning/material verification)
- REQ-COLLISION-003: Smooth separation ⚠️ (needs behavioral testing)
- REQ-COLLISION-004: Visual size matches collision radius ⚠️ (needs measurement)

### ❌ **Missing: Game Progression Systems**

1. **DNA Traits System** - Prerequisite-based unlock tree
2. **Nutrient Upgrades** - Scaling and multiplicative stacking
3. **Disinfection Abilities** - Active/passive/idle player capabilities
4. **Advanced Microbe Behaviors** - Membrane undulation, flagella waves, swarm coordination

### ❌ **Missing: Advanced Components**

- `ECMLocomotion.h` - Separate component (currently embedded in `Microbe`)
- `SDFRenderComponent.h` - Shader reference and uniform locations
- `Resource.h` - Biologically grounded resource types
- `Progression.h` - DNA traits and upgrade state

---

## **IMMEDIATE PRIORITY: Fix 3D Rendering Context Issue**

**Goal**: Fix why DrawSphere doesn't work in World::render() but DrawCircle works in World::renderUI().

**Success Criteria**: 3D microbe rendering appears in screenshots

#### **Step 1: Isolate the Issue**
- [x] Confirmed 2D rendering works in World::renderUI()
- [x] Confirmed 3D rendering fails in World::render() - even render-to-texture approach doesn't work
- [x] Root cause: OpenGL state corruption in World::render() context

#### **Step 2: Fix Rendering Context**
- [ ] Investigate Raylib's OpenGL state management between BeginDrawing/EndDrawing
- [ ] Try rendering 3D scene in World::renderUI() after 2D elements
- [ ] Test alternative approaches (multiple BeginDrawing/EndDrawing, different matrix setup)

#### **Step 3: Restore Proper 3D Rendering**
- [ ] Implement working 3D rendering solution
- [ ] Test: Basic 3D shapes appear
- [ ] Activate SDF raymarching for organic microbe visuals

**Current Status**: Visual tests working with 2D microbe circles. 3D rendering blocked by OpenGL context issues.
**Estimated Time**: 4-8 hours (Raylib OpenGL state management is complex)
**Risk**: High - May require deep Raylib/OpenGL expertise or workaround approaches

---

## Implementation Roadmap (Post-Rendering Fix)

### Phase 1: Core Architecture Verification

**Goal**: Ensure all systems follow FLECS pipeline phases correctly.

#### 1.1 Verify Phase Separation
- [ ] Confirm Input → Simulation → Transform Sync → Render order
- [ ] Test: Add debug logging to verify phase execution order
- [ ] Ensure no cross-phase dependencies

#### 1.2 Component Separation
- [ ] Extract `ECMLocomotion.h` from `Microbe.h`
- [ ] Create `SDFRenderComponent.h` with shader references
- [ ] Test: Verify component queries work correctly

#### 1.3 System Integration Testing
- [ ] Run full integration test suite
- [ ] Verify all systems work together without conflicts
- [ ] Test: Visual test passes with SDF rendering

**Success Criteria**: Clean architecture, all systems functional, SDF rendering active.

---

### Phase 2: Collision Physics Verification

**Goal**: Meet all collision requirements (Architecture §7.4).

#### 2.1 Material Tuning
- [ ] Review Jolt material properties (friction, restitution)
- [ ] Tune for soft, elastic, gel-like response
- [ ] Test: Verify REQ-COLLISION-002 (soft elastic response)

#### 2.2 Collision Radius Verification
- [ ] Measure SDF visual bounds
- [ ] Compare with Jolt collision shapes
- [ ] Adjust to ensure REQ-COLLISION-004 (visual size matches)
- [ ] Test: Visual collision overlay test

#### 2.3 Smooth Separation Verification
- [ ] Test collision scenarios with SDF rendering
- [ ] Verify REQ-COLLISION-003 (smooth separation)
- [ ] Tune restitution if needed

**Success Criteria**: All collision requirements verified with visual tests.

---

### Phase 3: Complete Game Loop

**Goal**: Full idle game mechanics (spawn → destroy → resources).

#### 3.1 Destruction Mechanics
- [ ] Implement proper ray casting for hover detection
- [ ] Add click-to-destroy functionality
- [ ] Test: Verify accurate microbe targeting

#### 3.2 Resource System Completion
- [ ] Implement collection mechanics
- [ ] Add biologically grounded resource types
- [ ] Test: Verify resource drops and collection

#### 3.3 Spawn Rate Balancing
- [ ] Tune spawn rates for idle gameplay
- [ ] Add progression-based spawn variety
- [ ] Test: Verify sustainable game pacing

**Success Criteria**: Complete idle game loop functional.

---

### Phase 4: DNA Traits & Progression

**Goal**: Implement core progression system (README §Progression).

#### 4.1 DNA Traits Tree
- [ ] Create prerequisite-based trait system
- [ ] Each trait provides: resource + structure + mechanic
- [ ] Implement trait unlocking logic

#### 4.2 Nutrient Upgrades
- [ ] Add scaling and multiplicative stacking
- [ ] Connect to resource collection
- [ ] Balance upgrade costs and effects

#### 4.3 Disinfection Abilities
- [ ] Implement active abilities (bursts, beams, chains)
- [ ] Add passive effects (fields, pulses)
- [ ] Create idle automation systems

**Success Criteria**: Full progression loop functional.

---

### Phase 5: Advanced Microbe Behaviors

**Goal**: Add realistic microbial motion and structures.

#### 5.1 Membrane Undulation
- [ ] Implement wave-based deformation
- [ ] Apply to soft body vertices
- [ ] Test: Verify organic movement

#### 5.2 Flagella & Appendages
- [ ] Add flagella wave propagation
- [ ] Implement pili twitching
- [ ] Create extracellular filaments

#### 5.3 Swarm Behaviors
- [ ] Add quorum sensing
- [ ] Implement coordinated movement
- [ ] Test: Verify group dynamics

**Success Criteria**: Microbes exhibit lifelike behaviors.

---

### Phase 6: Advanced Features & Polish

**Goal**: Complete game with defenses, offenses, and synergies.

#### 6.1 Defensive Structures
- [ ] Capsules (damage mitigation)
- [ ] Biofilms (armor + clustering)
- [ ] Spores (near-invulnerability)

#### 6.2 Offensive Structures
- [ ] Harpoon systems
- [ ] Contact toxins
- [ ] Swarm coordination

#### 6.3 Game Balancing & Polish
- [ ] Balance all systems
- [ ] Add visual polish
- [ ] Performance optimization

**Success Criteria**: Complete, balanced idle microbiology game.

---

## Development Rules & Practices

**MANDATORY** (Architecture §2 - Development Requirements):

- ✅ **REQ-001**: Root cause analysis - fix causes, not symptoms
- ✅ **REQ-002**: No compensatory mechanisms for design flaws
- ✅ **REQ-003**: Reject fixes that don't address root causes
- ✅ **REQ-004**: Build after every logical change
- ✅ **REQ-005**: Execute tests to verify correctness
- ✅ **REQ-006**: Confirm changes work before proceeding
- ✅ **REQ-007**: Refactor to reduce complexity continuously
- ✅ **REQ-008**: Maintain complexity neutrality or reduction
- ✅ **REQ-009**: Eliminate code smells during development
- ✅ **REQ-010**: Split files exceeding 1000 lines
- ✅ **REQ-011**: Replace inadequate algorithms entirely
- ✅ **REQ-012**: Use established libraries and techniques
- ✅ **REQ-013**: Consult literature for algorithm selection
- ✅ **REQ-014**: Try alternatives when approaches fail
- ✅ **REQ-015**: Accept no persistent failures
- ✅ **REQ-016**: Prioritize readability over cleverness
- ✅ **REQ-017**: Justify complexity by requirements
- ✅ **REQ-018**: Leave codebase cleaner than before
- ✅ **REQ-019**: No debug code in production logic

**Build & Test Requirements**:
- **Build**: `bin/build.sh` (cross-compile Windows .exe)
- **Test**: `bin/test.sh` (run tests + coverage)
- **Visual Validation**: `bin/vlm` must indicate suitable graphics
- **Coverage Target**: 95%+ on `/src` modules
- **Never run `game.exe`** during development - use `tests.exe`

---

## Key Reminders

1. **Visual Validation Mandatory**: Every graphical change must be validated with `bin/vlm`
2. **Architecture Compliance**: All changes must follow ARCHITECTURE.MD structure
3. **Puppet Architecture**: Physics drives rendering, SDF shows organic deformation
4. **Complexity Reduction**: Every change should simplify, not complicate
5. **Test-Driven**: All verification through proper tests, no ad-hoc debugging
6. **Root Cause Focus**: Address underlying issues, not surface symptoms
