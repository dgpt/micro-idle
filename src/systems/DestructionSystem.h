#ifndef MICRO_IDLE_DESTRUCTION_SYSTEM_H
#define MICRO_IDLE_DESTRUCTION_SYSTEM_H

#include <flecs.h>
#include "raylib.h"

namespace micro_idle {

struct PhysicsSystemState; // Forward declaration

// DestructionSystem - handles hover/click detection and microbe destruction
class DestructionSystem {
public:
    // Register the system with FLECS world
    static void registerSystem(flecs::world& world, PhysicsSystemState* physics);

    // Check if a point (mouse position) intersects with a microbe
    // Returns true if point is within microbe's collision radius
    static bool isPointInMicrobe(Vector3 point, Vector3 microbePos, float microbeRadius);

    // Apply damage to a microbe
    // Returns true if microbe was destroyed
    static bool applyDamage(flecs::entity microbeEntity, float damage);

    // Destroy a microbe and spawn resources
    static void destroyMicrobe(flecs::entity microbeEntity, flecs::world& world);
};

} // namespace micro_idle

#endif
