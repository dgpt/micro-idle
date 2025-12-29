# Micro-Idle Engine - Implementation Plan

## Current Status
- **Phase**: 4 - Basic Microbe System Working
- **Last Updated**: 2025-12-28
- **Status**: Physics, locomotion, and basic rendering working
- **Known Issues**:
  - Mesh looks low-poly (only 12 vertices, flat shading)
  - Constraint creation hangs with >12 vertices (needs optimization)
- **Completed**:
  - ✅ Fixed crash (ECMLocomotionSystem division by zero)
  - ✅ Mesh physics (cloth sim over skeleton) working
  - ✅ EC&M locomotion working
  - ✅ Mesh surface rendering (triangle drawing)
  - ✅ No gravity / Y-axis locked for 2D simulation

---

## Architecture Overview

**Tech Stack**:
- **Language**: C++20
- **Build**: CMake with FetchContent/Git submodules
- **Framework**: Raylib 5.5 (windowing, input, rendering)
- **Physics**: Jolt Physics (multi-threaded CPU)
- **ECS**: FLECS (Entity Component System)

**The Simulation Loop**:
1. **Input Phase**: Raylib polls input → writes to FLECS `InputComponent`
2. **Simulation Phase**:
   - `System_UpdatePhysics`: Steps Jolt world
   - `System_SyncTransforms`: Jolt → FLECS transform sync
   - `System_GameLogic`: FLECS queries for game state updates
3. **Render Phase**: FLECS queries → Raylib `DrawMesh()`

**Platforms**: Windows, Linux, macOS, Android, iOS

---

## Phase 1: Foundation Setup

### 1.1 Documentation
- [ ] Update README.md (FLECS, Jolt, C++20)
- [ ] Update ARCHITECTURE.MD (Simulation Loop pattern)
- [ ] Clean up old Bullet-related docs

### 1.2 CMake Configuration
- [ ] Set C++20 standard
- [ ] Add FLECS via FetchContent (https://github.com/SanderMertens/flecs)
- [ ] Add Jolt Physics via FetchContent (https://github.com/jrouwe/JoltPhysics)
- [ ] Keep Raylib 5.5 configuration
- [ ] Set optimization flags (-O3 for GCC/Clang, /O2 for MSVC)
- [ ] Configure static linking
- [ ] Remove Bullet dependencies

### 1.3 Directory Restructure
- [ ] Create `/extern` for submodules (if using git submodules)
- [ ] Create `/src/components` for FLECS components
- [ ] Create `/src/systems` for FLECS systems
- [ ] Remove old physics code (`game/physics.cpp`, `game/physics.h`, `game/xpbd.cpp`)
- [ ] Remove old microbe bodies (`game/microbe_bodies.cpp`, `game/microbe_bodies.h`)
- [ ] Clean up unused test files

### 1.4 Build Verification
- [ ] Build succeeds on current platform
- [ ] All tests pass
- [ ] Verify FLECS and Jolt link correctly

---

## Phase 2: FLECS Core Integration

### 2.1 Basic Components
- [ ] Create `src/components/Transform.h` (Position, Rotation, Scale)
- [ ] Create `src/components/Physics.h` (RigidBodyConfig, BodyID)
- [ ] Create `src/components/Rendering.h` (MeshComponent, ColorComponent)
- [ ] Create `src/components/Input.h` (InputComponent for cursor position)

### 2.2 World Setup
- [ ] Create `FlecsWorld` class wrapper
- [ ] Initialize FLECS world in `main.cpp`
- [ ] Set up FLECS pipeline phases (OnUpdate, OnStore, PostUpdate)
- [ ] Basic entity creation/destruction

### 2.3 Basic Systems
- [ ] `System_UpdateInput`: Raylib → FLECS
- [ ] `System_Render`: FLECS → Raylib (basic sphere rendering initially)
- [ ] Verify entity spawning and rendering works

---

## Phase 3: Jolt Physics Bridge

### 3.1 Jolt Initialization
- [ ] Create `src/systems/PhysicsSystem.h/cpp`
- [ ] Initialize Jolt `JPH::PhysicsSystem`
- [ ] Configure job system (multi-threading)
- [ ] Set up broad phase, object layers, contact listeners
- [ ] Store as FLECS singleton component

### 3.2 The Bridge (FLECS ↔ Jolt)
- [ ] Create FLECS Observer for `RigidBodyConfig` component
  - When added → create Jolt body, store `BodyID` on entity
- [ ] Create FLECS Observer for component removal
  - When removed → destroy Jolt body
- [ ] Implement `System_SyncTransforms`:
  - Read Jolt positions → write to FLECS `Transform`

### 3.3 Physics Update Loop
- [ ] `System_UpdatePhysics`: Step Jolt world each frame
- [ ] Handle FLECS → Jolt teleportation (gameplay overrides)
- [ ] Test with simple falling boxes or spheres

---

## Phase 4: Microbe Entity Migration

### 4.1 Microbe Components
- [ ] Create `src/components/Microbe.h`:
  - `MicrobeType` enum
  - `ECMLocomotion` component (phase, pseudopod state)
  - `MicrobeStats` (seed, color, size)

### 4.2 Soft Body Physics (Jolt)
- [ ] Research Jolt soft body support (or use multiple rigid bodies connected by constraints)
- [ ] Create amoeba as cluster of connected spheres (lattice structure)
- [ ] Configure Jolt distance constraints for deformability
- [ ] Test single amoeba deformation

### 4.3 EC&M Locomotion System
- [ ] Port EC&M algorithm to FLECS system
- [ ] `System_AmoebaBehavior`: Query entities with `ECMLocomotion`
- [ ] Apply forces to Jolt bodies based on cycle phase
- [ ] Verify amoeba movement works

---

## Phase 5: Rendering (Metaballs)

### 5.1 Particle Data Sync
- [ ] Create SSBO for particle positions (from Jolt soft body nodes)
- [ ] `System_SyncParticles`: Jolt → GPU SSBO
- [ ] Verify data reaches shader

### 5.2 Metaball Shaders
- [ ] Create `data/shaders/metaball.vert`
- [ ] Create `data/shaders/metaball.frag`
- [ ] Implement metaball field rendering (instanced billboards)
- [ ] Load shaders in rendering system

### 5.3 Microbe Renderer
- [ ] `System_RenderMicrobes`: Use metaball shaders
- [ ] Bind particle SSBO
- [ ] Draw instanced quads (one per particle)
- [ ] Verify organic blob appearance

---

## Phase 6: Scaling & Optimization

### 6.1 Performance Testing
- [ ] Spawn 100+ microbes
- [ ] Profile frame times
- [ ] Profile Jolt simulation time
- [ ] Identify bottlenecks

### 6.2 Jolt Multi-threading
- [ ] Configure Jolt job system for max CPU cores
- [ ] Test performance improvement
- [ ] Verify determinism (if needed)

### 6.3 FLECS Optimization
- [ ] Use FLECS queries efficiently (cache queries)
- [ ] Add/remove entities in batches
- [ ] Profile FLECS system overhead

---

## Phase 7: Additional Microbe Types

### 7.1 New Body Plans
- [ ] Stentor (trumpet shape)
- [ ] Lacrymaria (extendable neck)
- [ ] Heliozoa (radiating spines)
- [ ] Bacteria (simple spheres/rods)

### 7.2 Type-Specific Systems
- [ ] `System_StentorBehavior`
- [ ] `System_LacrymariaBehavior`
- [ ] Rendering variants (shader switches)

---

## Phase 8: Cross-Platform

### 8.1 Platform Abstractions
- [ ] `GetResourcePath()` wrapper (Desktop vs Android vs iOS)
- [ ] Main loop abstraction (while loop vs OS callback)
- [ ] Test on Linux

### 8.2 Mobile Preparation
- [ ] Android NDK build configuration
- [ ] iOS Xcode project generation
- [ ] Test build (if devices available)

---

## Key Files

**Docs**: README.md, ARCHITECTURE.MD, PLAN.md (this file)
**Build**: CMakeLists.txt, bin/build.sh
**Components**: src/components/*.h
**Systems**: src/systems/*.cpp
**Shaders**: data/shaders/metaball.*
**Main**: bin/main.cpp

---

## Cleanup Checklist

Files to DELETE:
- [ ] `game/physics.cpp` (old Bullet code)
- [ ] `game/physics.h` (old Bullet code)
- [ ] `game/xpbd.cpp` (legacy XPBD solver)
- [ ] `game/microbe_bodies.cpp` (old body plans)
- [ ] `game/microbe_bodies.h` (old body plans)
- [ ] Any Bullet-specific test files

Dependencies to REMOVE from CMake:
- [ ] Bullet Physics FetchContent
- [ ] All Bullet library links
- [ ] OpenCL detection (not needed with Jolt)
