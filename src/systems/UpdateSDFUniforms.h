#ifndef MICRO_IDLE_UPDATE_SDF_UNIFORMS_H
#define MICRO_IDLE_UPDATE_SDF_UNIFORMS_H

#include <flecs.h>

namespace micro_idle {

struct PhysicsSystemState; // Forward declaration

// UpdateSDFUniforms system - extracts soft body vertex positions and updates shader uniforms
// Runs in OnStore phase (after TransformSync, before rendering)
class UpdateSDFUniforms {
public:
    // Register the system with FLECS world
    static void registerSystem(flecs::world& world, PhysicsSystemState* physics);
};

} // namespace micro_idle

#endif
