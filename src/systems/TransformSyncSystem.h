#ifndef MICRO_IDLE_TRANSFORM_SYNC_SYSTEM_H
#define MICRO_IDLE_TRANSFORM_SYNC_SYSTEM_H

#include <flecs.h>

namespace micro_idle {

struct PhysicsSystemState; // Forward declaration

// Transform sync system - syncs Jolt physics transforms to FLECS Transform components
// Runs in OnStore phase (after physics update, before rendering)
class TransformSyncSystem {
public:
    // Register the transform sync system with FLECS world
    static void registerSystem(flecs::world& world, PhysicsSystemState* physics);
};

} // namespace micro_idle

#endif
