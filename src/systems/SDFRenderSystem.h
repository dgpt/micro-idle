#ifndef MICRO_IDLE_SDF_RENDER_SYSTEM_H
#define MICRO_IDLE_SDF_RENDER_SYSTEM_H

#include <flecs.h>
#include "raylib.h"

namespace micro_idle {

// SDF render system - renders microbes using SDF raymarching
// Runs in PostUpdate phase (final phase, after all simulation)
class SDFRenderSystem {
public:
    // Register the system with FLECS world
    static void registerSystem(flecs::world& world);
};

} // namespace micro_idle

#endif
