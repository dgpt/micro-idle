# Micro-Idle (Code-First C) — Stack & Project Structure Architecture

## 1) Objectives (Engineering Only)

1. **Pure code-first project**: no editor-authoritative state.
2. **Small runtime footprint**: C, no managed runtime.
3. **Deterministic core option**: project supports deterministic simulation modes and replayability by design (even if not always enabled).
4. **Scalable rendering**: thousands of instances, layered shading, lightweight VFX, predictable CPU/GPU costs.
5. **AI-friendly codebase**: explicit boundaries, consistent patterns, declarative data tables, minimal hidden coupling.

---

## 2) Fixed Stack

### Platform Layer

* **SDL3**

  * window creation, events, input devices
  * high-resolution timing
  * audio device (optional; may wrap with a dedicated mixer lib later)

### Renderer

* **bgfx**

  * cross-platform API (D3D/Metal/Vulkan/OpenGL)
  * explicit render passes / views
  * transient vertex/index buffers and instance buffers

### Game Structure

* **Flecs (C API)**

  * entities/components/systems
  * explicit “world” update stages

### Dev & Diagnostics

* **Dear ImGui**

  * runtime inspection, profiling HUD, live tweak panels
  * wired as an overlay render pass, not “part of” the engine state

### Optional Utility Libraries (recommended)

* **stb_image**, **stb_truetype** (asset loading convenience)
* **cgltf** (glTF ingestion at tools/offline step; runtime loading optional)
* **meshoptimizer** (offline mesh optimization)
* **xxhash** or similar (fast content hashing for caching/pipelines)

---

## 3) Repository Layout (Hard Contract)

Use a structure that keeps module dependencies one-directional.

```
/MicroIdle
  /apps
    /game              # shipped executable
    /tools             # offline tools (asset baking, packer, shaders)
  /engine
    /platform          # SDL3 glue: window, events, timers, file I/O abstraction
    /renderer          # bgfx wrapper: resources, passes, pipelines, shaders
    /ecs               # Flecs integration: world init, staging, reflection helpers
    /assets            # runtime asset manager + cache
    /io                # serialization, streaming, virtual filesystem
    /math              # vectors/matrices/quats, deterministic helpers
    /util              # logging, asserts, allocators, hash, arenas
    /debug             # ImGui integration, debug draw, capture tools
  /game               # (optional) separate from engine; project-specific code
  /data
    /raw               # source assets (png, gltf, wav, fonts)
    /shaders           # shader sources (bgfx shaderc inputs)
    /baked             # generated outputs (packed, compressed, versioned)
  /third_party
    /SDL3
    /bgfx
    /flecs
    /imgui
    /stb
    /cgltf
  /build
  /scripts
  CMakeLists.txt (or Meson)
```

**Rule:** `apps/game` may depend on `engine/*` and `game/*`.
`engine/*` must **never depend on** `apps/*`.
`renderer` must not depend on `game`.

---

## 4) Dependency Rules (Enforced)

Define a strict dependency DAG:

* `engine/util` → nothing
* `engine/math` → `util`
* `engine/io` → `util`
* `engine/platform` → `util`, `io`
* `engine/assets` → `util`, `io`, `platform`
* `engine/renderer` → `util`, `math`, `platform`, `assets` (read-only)
* `engine/ecs` → `util` (+ Flecs), may reference `math` types
* `engine/debug` → `util`, `renderer`, `platform`, `ecs` (+ ImGui)

**No module may include headers from a “higher layer”.**
This is crucial for keeping the codebase AI-automatable without cascading breakage.

---

## 5) Build System & Tooling (Recommended)

### Build System

Prefer **CMake** or **Meson**. Either is fine; pick one and commit.

Required features:

* unity build toggle (`-DUNITY_BUILD=ON/OFF`)
* sanitizers toggle (`-DSANITIZE=address,undefined`)
* LTO toggle (`-DLTO=ON`)
* static analysis target (clang-tidy if available)
* shader build step integrated

### Shader Pipeline (bgfx)

* Shader sources live in `/data/shaders/src`
* Build step runs `shaderc` to generate platform-specific binaries into `/data/baked/shaders/<platform>/...`
* Runtime loads from baked output only

### Offline Asset Baking (Tools App)

Create `/apps/tools` with commands like:

* `pack_assets` (packs textures/meshes/fonts into a single archive)
* `build_atlas` (optional)
* `compile_shaders` (invoked by build or as a tool)

**The runtime should not “discover” assets by walking folders.**
It loads from a **versioned asset pack** (see next section).

---

## 6) Asset System (Runtime)

### Goals

* fast loads
* minimal file I/O calls
* stable identifiers
* hot reload support in dev

### Virtual File System (VFS)

Implement an abstraction in `engine/io`:

* `vfs_mount_dir(path)`
* `vfs_mount_pack(path)`
* `vfs_open(uri)`
* `vfs_read(handle, ...)`

In dev, mount both:

* `/data/raw` (for iteration)
* `/data/baked` (authoritative output)

In release, mount only:

* `/data/baked/assets.pack`

### Asset IDs

Every asset is addressed by a stable ID:

* `AssetId` = 64-bit hash of a canonical path like `tex/ui/foo.png`
* Store mapping `path -> id` in the pack manifest for debugging.

### Runtime Asset Manager (`engine/assets`)

Responsibilities:

* async-ish staging (but deterministic ordering option)
* dedup by `AssetId`
* lifetime tracking (refcount or explicit handles)
* interface for renderer to create GPU resources

Keep the API “flat”:

```c
TextureHandle assets_get_texture(AssetId id);
MeshHandle    assets_get_mesh(AssetId id);
FontHandle    assets_get_font(AssetId id);
```

The renderer module owns GPU-side objects; assets owns decoding + CPU caches.

---

## 7) Memory Management Strategy

The entire project should be predictable under load.

### Allocators (engine/util)

Provide:

* **arena allocator** (frame arena and long-lived arenas)
* **pool allocator** (fixed-size objects)
* optional: **TLSF** or mimalloc for general heap (not required)

Recommended conventions:

* `FrameArena` resets every frame
* `SimArena` persists across loads
* asset decode uses a dedicated `AssetArena`
* ECS component storage is in Flecs; avoid per-entity heap allocations

### “No heap churn per frame” guideline

* any per-frame temporary: frame arena
* any persistent: sim arena or asset cache

---

## 8) ECS Integration Structure (Flecs)

Put all Flecs wiring in `engine/ecs`:

* world creation/destruction
* system registration
* staging & pipeline order

### Recommended Flecs “stages” (generic)

Define named phases that are **project-agnostic**:

1. `Phase_PreUpdate`
2. `Phase_Update`
3. `Phase_PostUpdate`
4. `Phase_PreRender`
5. `Phase_RenderSubmit` (gathers renderables into render queues)
6. `Phase_PostRender`

**Critical:** the renderer should consume **render queues** generated by ECS systems, not query arbitrary components itself (keeps boundaries clean).

### Render Queue Components

Create a small bridge in `engine/renderer` / `engine/ecs`:

* `Renderable` component contains handles and per-instance params
* `RenderLayer` / `MaterialKey` is a compact descriptor
* systems fill a `RenderQueue` each frame (in frame arena)

Renderer then:

* sorts by pipeline/material
* submits to bgfx

---

## 9) Rendering Architecture (bgfx wrapper)

### Renderer Module (`engine/renderer`)

Split into:

* `renderer_device.*`

  * bgfx init/shutdown
  * platform integration (SDL window handle)
  * frame begin/end

* `renderer_resources.*`

  * texture creation, mesh creation
  * shader/program creation
  * uniform management
  * pipeline/material registry

* `renderer_passes.*`

  * view definitions
  * clear states
  * render order and per-pass submission

* `renderer_draw.*`

  * high-level draw submission from render queue
  * instancing path
  * debug draw path

### Pass Model (generic)

Keep it minimal but extensible:

1. `Pass_World` (3D scene)
2. `Pass_Overlay` (2D overlays, markers, highlights)
3. `Pass_UI` (fonts, panels, HUD)
4. `Pass_Debug` (lines, bounds, instrumentation)

This supports visuals being both decorative and explanatory because overlay + UI are explicit and controllable.

### Materials & “Layers”

Represent shading as a **material key** referencing:

* shader program
* blend mode
* depth state
* textures/uniform blocks

Allow multiple material layers per entity (e.g., shell + interior + emissive).
Do it via multiple `Renderable` entries or a `Renderable` with N sub-draws—pick one and standardize.

### Instancing

Use bgfx instancing where possible:

* instance data layout standardized (mat4 + packed params)
* provide a “packed params” uniform buffer for per-instance variability (colors, thresholds, animation phases)

---

## 10) Timing Model

### Two clocks

* `RealTime` (actual frame delta)
* `SimTime` (fixed tick; optionally deterministic)

In `engine/platform/time.*`:

* implement fixed-step accumulator
* simulation updates run at `tick_dt` (e.g., 1/60)
* renderer runs every frame and interpolates if desired

Even if you don’t always need fixed-step, having this structure prevents time-dependent nondeterminism from leaking everywhere.

---

## 11) Serialization & Versioning

Implement in `engine/io` and `engine/save`:

### File formats

* Use a small binary format with:

  * magic
  * version
  * endian marker
  * section table

### Versioning policy

* Every serialized blob has an explicit version
* Migration functions live next to the struct definitions
* Avoid reflection-based serialization

### Content hashing

* Hash baked asset pack + shader pack versions
* At startup, log what exact pack versions were loaded

This makes reproducing runs and debugging significantly easier.

---

## 12) Logging, Assertions, Crash Capture

In `engine/util`:

* `LOG_INFO/WARN/ERROR`
* `ASSERT(x)` with message
* optional crash handler (minidump on Windows, signal handler on Linux)

In `engine/debug`:

* on-screen console (dev builds)
* GPU/CPU timing graph (bgfx stats + your own timers)

---

## 13) Testing Strategy

### Unit tests (C)

* Put under `/apps/tests`
* Test:

  * math primitives
  * hashing/stable IDs
  * pack file reading
  * determinism of PRNG streams (if used)

### Golden tests (high value)

* Run a headless build (or minimal window) that:

  * loads asset packs
  * runs N ticks
  * outputs a stable checksum of critical state blobs
* Compare against committed “goldens” in CI

This is the single most effective way to keep a codebase AI-editable without regressions.

---

## 14) CI / Build Artifacts

Build matrix:

* Windows (MSVC or clang-cl)
* Linux (clang)
* optional: macOS (clang)

Artifacts:

* `game` executable
* `assets.pack`
* `shaders/<platform>/...`
* `manifest.json` with versions and hashes

---

## 15) Coding Standards (to keep it AI-friendly)

* C11, no compiler extensions unless wrapped
* Headers expose *minimal* types; prefer opaque handles
* No global singletons except `EngineContext* ctx` passed explicitly
* Each module has:

  * `module.h` (public API)
  * `module_internal.h` (private)
  * `module.c` (implementation)
* Avoid cyclic includes; enforce include order

---

## 16) “Engine Context” Pattern (Recommended)

Central struct defined in `/engine/engine.h`:

```c
typedef struct EngineContext {
  Platform* platform;
  Renderer* renderer;
  Assets*   assets;
  ecs_world_t* ecs;

  // arenas
  Arena frame_arena;
  Arena sim_arena;

  // clocks
  TimeState time;

  // config
  EngineConfig config;
} EngineContext;
```

Apps use:

* `engine_init(&ctx, config)`
* `engine_tick(&ctx)`
* `engine_shutdown(&ctx)`

Everything flows through `EngineContext` so dependencies remain explicit.

---

## 17) Minimal “Frame Flow” (Structure Only)

1. `platform_pump_events()`
2. `time_update()`
3. While accumulator >= tick:

   * `ecs_progress(world, tick_dt)` in deterministic phase order
4. `ecs_progress(world, render_dt)` for PreRender/RenderSubmit (or separate)
5. `renderer_begin_frame()`
6. `renderer_submit(queue)`
7. `imgui_submit()`
8. `renderer_end_frame()`
9. `frame_arena_reset()`

No subsystem is allowed to secretly advance time or query “now” outside `TimeState`.


aHere is a **single, clean, consolidated prompt** that captures the entire game clearly and concisely, suitable for handing to an AI, collaborator, or future-you:

---

## **Game Prompt: “Micro-Idle”**

Design an **idle / hover-to-destroy game about microbiology** where the player views a **top-down (orthographic or shallow-perspective) 3D world** of **procedurally generated microbes**, bursts them by hovering or clicking, and collects **biologically grounded resources** that feed into **three tightly interlocking progression systems**.

### **Core Loop**

1. Procedurally generate microbes deterministically from a **seed + unlocked traits**.
2. Render them as **animated 3D organisms** with readable biological structure (membranes, interiors, appendages).
3. Player hovers or attacks → microbes burst → **multiple realistic resources drop**.
4. Resources unlock deeper traits, nutrients, and disinfection abilities.
5. Unlocks alter **appearance, behavior, drop tables, and mechanics**—never cosmetic-only.

---

## **Procedural Microbes**

* Generated via a **stochastic shape grammar** (rules + weights + constraints).
* Deterministic: same seed + unlocks → identical microbe (persisted genotype).
* Base body shapes: **coccus, bacillus, vibrio, spirillum**.
* Visible layers unlock one-by-one:

  * Semi-transparent membranes / capsules
  * Internal nucleoid blobs, ribosomes, granules
  * Appendages: flagella, pili/fimbriae, EPS filaments
  * VFX layers: signal pulses, toxin auras, harpoon ports
* Animation is procedural and stable (no physics chaos):

  * Membrane undulation
  * Flagella traveling waves
  * Pili jitter
  * EPS drift
  * Signal pulses
  * Harpoon firing
  * Spore dormancy ↔ germination transitions

---

## **Resource Types (All Realistic)**

`Sodium, Glucose, Iron, Calcium, Lipids, Oxygen, Signaling Molecules`

Resources **do not exist until biologically justified traits are unlocked**.

---

## **Progression System 1 — DNA Traits (Discovery Tree)**

A prerequisite-based trait tree where **each trait**:

* Unlocks a **new resource**
* Adds a **new drop roll**
* Adds **exactly one visible microbe layer**
* Adds **exactly one gameplay mechanic**

Examples:

* **Capsule → Sodium** (survivability, chain reactions)
* **Flagella → Glucose** (movement, spawn density)
* **Pili → Iron** (adhesion, elite chance)
* **Endospores → Calcium** (dormancy, burst rewards)
* **LPS Layer → Lipids** (toxins, immune evasion)
* **Photosynthesis → Oxygen** (ambient generation, burn effects)
* **Quorum Sensing → Signals** (group coordination, chaining)

Traits branch into advanced behaviors like swarm logic, chemotaxis, conjugation, toxin bursts, and coordinated attacks.

---

## **Progression System 2 — Nutrients (Free-Form Scaling)**

Once unlocked, nutrients can be upgraded in any order:

* **Sodium:** larger/longer chain reactions
* **Glucose:** higher damage + spawn density
* **Iron:** >100% drop multiplicity, elite microbes
* **Calcium:** armor windows, passive scaling
* **Lipids:** pickup magnetism, toxin resistance
* **Oxygen:** burn effects, faster game tempo
* **Signals:** cooldown reduction, synergy amplification

Late-game introduces **cross-feeding and conversions** to keep economies entangled.

---

## **Progression System 3 — Disinfection (Player Abilities)**

Three parallel trees that define playstyle; players may hybridize.

### **Active**

Skill-based attacks (aimed bursts, beams, chain explosions).

### **Passive**

Environmental control (acid fields, AoE pulses, debuff fog).

### **Idle / Automation**

Autonomous killers, fever waves, offline digestion factories.

Abilities synergize across trees (marks amplify pulses, lightning ignites acid, fog boosts patrol crits).

---

## **Enemies with Real Mechanics**

Microbes gain **defense and offense structures** through grammar branches:

**Defense**

* Capsules (shielding)
* Biofilms (armor + clumping)
* Spores (near-immunity, delayed rewards)

**Offense**

* Harpoon systems (burst damage)
* Contact toxins (auras / DoT)
* Swarm coordination (buffed groups)

Every threat is **visually legible**.

---

## **Technical Constraints**

* Top-down camera (ortho preferred)
* Deterministic generation & animation
* One visible layer per unlock
* Modular Rust architecture (core logic decoupled from rendering)
* Bacteria-first realism (no eukaryotic cilia unless later branch)

---

## **Tone & Goal**

Educational but not preachy.
Readable, alive-feeling microbes.
Deep systemic progression without visual noise.
An idle game where **biology is the tech tree**.


