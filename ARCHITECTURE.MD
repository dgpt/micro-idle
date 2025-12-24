# Micro-Idle (raylib / C) — Stack & Project Structure Architecture

## 1) Goals (Engineering Only)

* **Pure code-first**: everything is expressed as code + data files, not editor state.
* **Small runtime footprint**: C, minimal dependencies, single binary + data pack.
* **High performance**: stable frame pacing, scalable draw submission, minimal heap churn.
* **Determinism-ready**: the project structure supports deterministic simulation/replay modes, even if not always enabled.
* **AI-friendly structure**: explicit module boundaries, consistent patterns, minimal hidden coupling.
* **Visuals are first-class**: rendering supports layered visuals, stylization, and debug overlays as peers.

---

## 2) Stack Choice

### Core

* **C (C11+)**
* **raylib** (window/input/audio/rendering)

  * Use raylib for:

    * Window + input
    * Texture/mesh/shader management
    * 2D/3D rendering
    * RenderTexture-based postprocessing
    * Basic audio (or integrate a mixer later)

### Optional but strongly recommended

* **Flecs** (C ECS), if you want ECS structure without building your own entity store.

  * You *can* do a custom struct-of-arrays entity store instead; the architecture below supports both.
* **stb_image / stb_truetype** only if you want custom loaders beyond raylib defaults.
* **cgltf** for offline glTF import (optional; can also just use raylib’s supported model formats).
* **xxhash** or similar for fast asset IDs & pack hashing.

**No managed runtime. No editor. No scripting language required.**

---

## 3) Repository Layout (Hard Contract)

Keep “engine-ish” code separate from “project code” even if this is a single game. This makes LLM edits safer and avoids monolith sprawl.

```
/MicroIdle
  /apps
    /game                 # main executable
    /tools                # offline tools: packer, shader preprocess, atlas builder
    /tests                # unit/golden tests
  /engine
    /platform             # raylib-facing platform wrappers: window, input mapping, timing
    /render               # renderer abstraction on top of raylib (passes, layers, materials)
    /assets               # asset manager + pack reader + caching
    /io                   # vfs, serialization, binary formats, compression
    /math                 # vec/mat/quats, color utils, hashing helpers
    /util                 # logging, asserts, allocators, profiling, arenas
    /debug                # debug draw, dev console, capture tools, inspector UI
    /ecs                  # (optional) Flecs integration or custom entity store
  /game                   # project-specific code (may depend on engine only)
  /data
    /raw                  # source assets (png, wav, fonts, models)
    /shaders              # shader sources (glsl)
    /baked                # baked outputs (packed archive, atlases, preprocessed shaders)
  /third_party            # raylib, optional libs
  /scripts                # build/run/pack scripts
  CMakeLists.txt (or Meson)
  ARCHITECTURE.md
```

### Dependency rules (enforced)

* `/engine/*` does **not** include `/game/*`
* `/apps/game` depends on `/engine` and `/game`
* `/engine/render` must not depend on `/game`
* `/engine/assets` must not depend on `/engine/render` (assets produce handles; render consumes them)

---

## 4) Build System

Prefer **CMake** (or Meson). Required targets:

* `game` (release)
* `game_dev` (dev: hot reload, debug UI, verbose logs)
* `tools/*` (asset packer, atlas builder, shader preprocessor)
* `tests` (unit + golden)

Build toggles:

* `-DDEV=ON/OFF` (enables debug UI, asserts, hot reload)
* `-DSANITIZE=address,undefined`
* `-DLTO=ON`
* `-DDETERMINISM=ON/OFF` (compiles deterministic time/PRNG paths)

---

## 5) Engine Context Pattern (Explicit Wiring)

Avoid globals. Centralize state through an `EngineContext`.

```c
typedef struct EngineConfig {
    int window_w, window_h;
    int target_fps;      // render target
    int tick_hz;         // sim tick (determinism-ready)
    bool vsync;
    bool dev_mode;
} EngineConfig;

typedef struct EngineContext {
    EngineConfig cfg;

    // subsystems
    struct Platform* platform;
    struct Assets*   assets;
    struct Render*   render;
    struct Debug*    debug;

    // optional ECS
    void* ecs_world;

    // memory
    struct Arena frame_arena;
    struct Arena sim_arena;

    // timing
    struct TimeState time;

} EngineContext;
```

Apps call:

* `engine_init(&ctx, cfg)`
* `engine_tick(&ctx)`
* `engine_shutdown(&ctx)`

This makes dependency and ownership obvious for AI edits.

---

## 6) Memory Strategy (Performance & Predictability)

### Allocators (engine/util)

Provide at least:

* **frame arena** (reset every frame)
* **sim arena** (long-lived game/session allocations)
* optional: pool allocator for fixed-size objects

Rules:

* *No per-frame malloc/free* in hot paths.
* Any transient lists (render queues, temp arrays) use the **frame arena**.
* Asset decode/caches use a dedicated **asset arena** or heap inside `Assets`.

---

## 7) Time Model (Determinism-Ready)

Maintain two clocks:

* **RealTime**: actual frame delta from raylib (`GetFrameTime()`).
* **SimTime**: fixed tick accumulator (optional determinism mode).

TimeState:

```c
typedef struct TimeState {
    double real_dt;
    double accumulator;
    double tick_dt;     // 1 / tick_hz
    uint64_t tick;      // monotonically increasing
} TimeState;
```

Frame flow:

1. read input/events
2. update `TimeState`
3. while accumulator >= tick_dt:

   * run fixed-step update stages
   * tick++
4. render with interpolation factor if desired
5. reset frame arena

Even if you don’t use deterministic mode initially, this structure prevents time leakage later.

---

## 8) Assets & Data Pipeline (Runtime + Tools)

### 8.1 Virtual File System (VFS)

Implement `engine/io/vfs.*` to abstract reading from:

* directory mounts (dev)
* pack archive (release)

API:

* `vfs_mount_dir("data/raw")`
* `vfs_mount_pack("data/baked/assets.pack")`
* `vfs_read_all("tex/ui/foo.png", &bytes, &len)`

### 8.2 Asset IDs

Use stable IDs:

* `AssetId = 64-bit hash("tex/ui/foo.png")`
* pack includes a manifest mapping id ↔ path for debugging

### 8.3 Runtime Asset Manager (`engine/assets`)

Responsibilities:

* cache & lifetime management for:

  * textures, models, fonts, sounds, shaders
* decode + upload to GPU through raylib
* optional hot reload in dev (file watching or timestamp polling)

Suggested API:

```c
Texture2D assets_texture(Assets*, AssetId);
Model     assets_model(Assets*, AssetId);
Shader    assets_shader(Assets*, AssetId);
Font      assets_font(Assets*, AssetId);
Sound     assets_sound(Assets*, AssetId);
```

**Ownership rule:** assets owns raylib handles; render only references them.

### 8.4 Offline tools (`/apps/tools`)

Create tools that generate `/data/baked`:

* `pack_assets`: pack raw assets into one archive (+ manifest)
* `build_atlas`: pack many sprites into atlases + UV tables
* `preprocess_shaders`: normalize shader sources and emit final glsl variants
* optional: `import_models`: convert/optimize models for faster runtime loads

Runtime should prefer baked assets; dev mode may fall back to raw.

---

## 9) Rendering Architecture (raylib-based, layered visuals)

raylib gives you immediate mode drawing calls, but you still want a structured renderer module so visuals remain composable and scalable.

### 9.1 Render module layout

```
/engine/render
  render_api.h              # public facade
  render_device.c           # init/shutdown, window sizes, begin/end frame
  render_passes.c           # pass graph and RenderTexture pipeline
  render_queue.c            # render queue building/sorting
  render_materials.c        # shader/material registry and parameter packing
  render_debug.c            # debug draw channel integration
```

### 9.2 Pass graph (simple, extensible)

Use `RenderTexture2D` to implement passes:

1. **Pass_World3D** → draws 3D
2. **Pass_Overlay** → 2D/3D overlays (markers, highlights, lines)
3. **Pass_PostFX** → bloom/outline/color grade/etc (optional chain)
4. **Pass_UI** → 2D UI
5. **Pass_Debug** → dev overlays

Implementation sketch:

* `BeginTextureMode(worldRT)` → `BeginMode3D(camera)` → draw 3D queue → end
* `BeginTextureMode(overlayRT)` → draw overlay queue → end
* `BeginTextureMode(postRT)` → apply shaders: worldRT + overlayRT → end
* `BeginDrawing()` → composite final RT to screen → UI → debug → `EndDrawing()`

This gives you clean separation and makes decorative/explanatory visuals a first-class pipeline.

### 9.3 Materials & shaders

Wrap raylib Shader usage behind a small “material system” to avoid ad-hoc uniform spam.

Define:

```c
typedef struct MaterialKey {
    uint32_t shader_id;
    uint32_t blend_mode;
    uint32_t flags; // e.g. depth, cull, wireframe, etc.
} MaterialKey;

typedef struct MaterialParams {
    // packed floats/vec4s; avoid pointers
    float p0[4];
    float p1[4];
    float p2[4];
} MaterialParams;
```

Renderables reference:

* geometry handle (mesh/model)
* transform
* MaterialKey
* MaterialParams

Renderer handles:

* grouping by MaterialKey
* setting uniforms in batches
* minimizing state changes

### 9.4 Render queue (data-driven)

Avoid “drawing directly from systems.” Instead:

* gather renderables into a `RenderQueue` each frame (frame arena)
* sort/group
* execute

This is AI-friendly and keeps the render pipeline consistent.

---

## 10) ECS / Data-Oriented Structure (Two viable paths)

### Path A: Flecs (recommended if you want easy scaling)

`/engine/ecs` provides:

* `ecs_init_world()`
* register phases/stages
* register components

Render submission is a stage:

* systems write to `RenderQueue` (frame arena)
* renderer consumes queue

### Path B: Custom SoA entity store (small & ultra-fast)

If you want minimal dependencies:

* implement `EntityId` + “dense arrays per component”
* explicit system loops

Keep the same boundary:

* systems populate render queue
* render module is independent

**Rule:** ECS/store owns game state; render consumes a queue.

---

## 11) Debug & Inspection (Dev Mode)

`/engine/debug` hosts:

* on-screen metrics (FPS, frame time, memory)
* live inspection panels
* capture tools (screenshots, frame dumps, optional)

Even without ImGui, you can:

* implement a simple dev console and overlay in raylib
* but ImGui can be integrated as a render pass if you prefer

**Important:** debug must not become a dependency sink; it depends on others, not vice versa.

---

## 12) Serialization & Versioning

Even if you don’t implement saves immediately, define the policy now:

* binary format with:

  * magic
  * version
  * endian marker
  * section table
* migrations are explicit functions
* no “serialize by pointer chasing”

Keep in `/engine/io` + `/engine/save`.

---

## 13) Testing & Golden Runs (LLM-proofing)

### Unit tests

* hashing / AssetId stability
* VFS read correctness
* pack reader validation
* math primitives

### Golden tests

Create a `tests/golden` runner that:

* runs N fixed ticks
* outputs a deterministic checksum of selected state blobs
* compares to committed expected outputs

This is the best way to safely let an LLM change code without silently breaking reproducibility.

---

## 14) Minimal Frame Flow (Structure Only)

**Main loop skeleton** (conceptual):

1. `platform_poll_input()`
2. `time_update()`
3. `while (accumulator >= tick_dt) { ecs_update_fixed(); accumulator -= tick_dt; }`
4. `ecs_update_prerender()` (build queues)
5. `render_execute(pass_graph, queues)`
6. `debug_execute()`
7. `frame_arena_reset()`

All subsystems receive `EngineContext*` and do not access globals.

---

## 15) Coding Standards to Keep It AI-Friendly

* C11, no compiler-specific magic unless isolated
* strict header hygiene:

  * public headers include only what they need
  * no cyclic includes
* opaque handles where possible
* consistent naming:

  * `assets_*`, `render_*`, `platform_*`, `ecs_*`
* “flat APIs” over clever abstractions
* every module has:

  * `module.h` (public)
  * `module.c` (impl)
  * optional `module_internal.h`

---

## 16) What raylib replaces vs the earlier SDL3+bgfx plan

* **platform**: raylib wraps window/events/input; you’ll build only thin “platform mapping” helpers
* **renderer**: no bgfx; raylib is the immediate renderer + shaders + RenderTextures
* **asset loading**: raylib loads textures/models/fonts/sounds directly; you still want an asset manager to enforce caching + pack loading
* **postprocessing**: implemented via `RenderTexture2D` + full-screen quad draws + raylib shaders

This reduces complexity, speeds prototyping, and still supports a disciplined modular architecture.
