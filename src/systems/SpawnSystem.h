#ifndef MICRO_IDLE_SPAWN_SYSTEM_H
#define MICRO_IDLE_SPAWN_SYSTEM_H

#include <flecs.h>
#include "raylib.h"
#include "../SpawnRequest.h"

namespace micro_idle {

class World; // Forward declaration

// SpawnSystem - handles procedural microbe generation
// Spawns microbes based on progression state and spawn rate
class SpawnSystem {
public:
    // Register the system with FLECS world
    static void registerSystem(flecs::world& world, World* worldInstance);

    // Generate a spawn request (for deferred spawning)
    static SpawnRequest generateSpawnRequest(float worldWidth,
                                             float worldHeight,
                                             float spawnHeight);

    // Spawn a single microbe at a random position within bounds
    // Returns the created entity
    static flecs::entity spawnMicrobe(World* worldInstance,
                                      float worldWidth,
                                      float worldHeight,
                                      float spawnHeight);

    // Get current spawn rate (microbes per second)
    static float getSpawnRate();

    // Set spawn rate
    static void setSpawnRate(float rate);

private:
    static float spawnRate;  // Current spawn rate (microbes per second)
    static float spawnAccumulator;  // Accumulated time since last spawn
};

} // namespace micro_idle

#endif
