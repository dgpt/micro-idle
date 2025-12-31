#include "SpawnSystem.h"
#include "src/components/Microbe.h"
#include "src/components/Transform.h"
#include "src/components/WorldState.h"
#include "src/World.h"
#include <cstdlib>
#include <cmath>

namespace micro_idle {

float SpawnSystem::spawnRate = 1.0f;  // Default: 1 microbe per second
float SpawnSystem::spawnAccumulator = 0.0f;

void SpawnSystem::registerSystem(flecs::world& world, World* worldInstance) {
    // System that spawns microbes based on spawn rate
    // Runs in OnUpdate phase (simulation phase)

    world.system("SpawnSystem")
        .kind(flecs::OnUpdate)
        .run([worldInstance](flecs::iter& it) {
            float dt = it.delta_time();

            static int callCount = 0;


            spawnAccumulator += dt;

            // Calculate how many microbes to spawn this frame
            float spawnInterval = 1.0f / spawnRate;
            int spawnCount = 0;

            // Spawn as many as we can (handle floating point precision issues)
            while (spawnAccumulator >= spawnInterval - 0.0001f) {  // Small epsilon for floating point
                spawnCount++;
                spawnAccumulator -= spawnInterval;
            }

            // Disabled excessive logging for performance
            // if (callCount < 20 || spawnCount > 0) {
            //     printf("SpawnSystem [call %d]: spawnAcc=%.6f (AFTER add), spawnCount=%d\n",
            //            callCount, spawnAccumulator + (spawnCount * spawnInterval), spawnCount);
            // }
            callCount++;

            if (spawnCount == 0) {
                return;  // Nothing to spawn this frame
            }

            // Get world bounds from singleton
            auto worldState = it.world().get<components::WorldState>();
            if (worldState && !worldState->spawnEnabled) {
                return;
            }

            float worldWidth = worldState ? worldState->worldWidth : 50.0f;
            float worldHeight = worldState ? worldState->worldHeight : 50.0f;
            float spawnHeight = 1.5f;  // Spawn near ground for cohesive visuals


            // Queue spawn requests (will be executed after world.progress() to avoid readonly issues)
            for (int i = 0; i < spawnCount; i++) {
                SpawnRequest request = generateSpawnRequest(worldWidth, worldHeight, spawnHeight);
                worldInstance->spawnQueue.push_back(request);
            }

            // CRITICAL: Flush deferred operations so entities are immediately created
            // This is necessary because entity.set() calls within systems are deferred
            // Note: defer_end() may not work during readonly mode, so we may need to defer spawning
            // it.world().defer_end();
        });
}

SpawnRequest SpawnSystem::generateSpawnRequest(float worldWidth,
                                                float worldHeight,
                                                float spawnHeight) {
    // Random position within bounds (accounting for microbe radius)
    float margin = 2.0f;  // Margin from walls
    float halfWidth = worldWidth / 2.0f - margin;
    float halfHeight = worldHeight / 2.0f - margin;

    float randX = (float)rand() / RAND_MAX;
    float randZ = (float)rand() / RAND_MAX;
    float x = randX * 2.0f * halfWidth - halfWidth;
    float z = randZ * 2.0f * halfHeight - halfHeight;
    float y = spawnHeight;

    Vector3 position = {x, y, z};

    // Random radius (0.8 to 1.4)
    float radius = 0.8f + ((float)rand() / RAND_MAX) * 0.6f;

    // Random color (variation of green/blue for microbes)
    Color color = {
        (unsigned char)(90 + (rand() % 60)),
        (unsigned char)(170 + (rand() % 60)),
        (unsigned char)(110 + (rand() % 60)),
        255
    };

    return SpawnRequest{position, radius, color};
}

flecs::entity SpawnSystem::spawnMicrobe(World* worldInstance,
                                        float worldWidth,
                                        float worldHeight,
                                        float spawnHeight) {
    if (!worldInstance) {
        return flecs::entity();
    }

    SpawnRequest request = generateSpawnRequest(worldWidth, worldHeight, spawnHeight);

    // Create amoeba using World factory method
    return worldInstance->createAmoeba(request.position, request.radius, request.color);
}

float SpawnSystem::getSpawnRate() {
    return spawnRate;
}

void SpawnSystem::setSpawnRate(float rate) {
    spawnRate = rate > 0.0f ? rate : 0.0f;
}

} // namespace micro_idle
