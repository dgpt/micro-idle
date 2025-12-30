#ifndef MICRO_IDLE_RESOURCE_SYSTEM_H
#define MICRO_IDLE_RESOURCE_SYSTEM_H

#include <flecs.h>
#include "src/components/Resource.h"
#include "raylib.h"

namespace micro_idle {

// ResourceSystem - handles resource drops, collection, and lifetime
class ResourceSystem {
public:
    // Register the system with FLECS world
    static void registerSystem(flecs::world& world);

    // Spawn a resource drop at a position
    static flecs::entity spawnResource(flecs::world& world,
                                      components::ResourceType type,
                                      float amount,
                                      Vector3 position);

    // Collect a resource (add to inventory and mark for removal)
    static void collectResource(flecs::entity resourceEntity, flecs::world& world);
};

} // namespace micro_idle

#endif
