#include "DestructionSystem.h"
#include "src/components/Microbe.h"
#include "src/components/Transform.h"
#include "src/components/Resource.h"
#include "src/components/Input.h"
#include "src/systems/PhysicsSystem.h"
#include "src/systems/ResourceSystem.h"
#include "raylib.h"
#include <cmath>
#include <cstdio>

namespace micro_idle {

bool DestructionSystem::isPointInMicrobe(Vector3 point, Vector3 microbePos, float microbeRadius) {
    float dx = point.x - microbePos.x;
    float dy = point.y - microbePos.y;
    float dz = point.z - microbePos.z;
    float distSq = dx * dx + dy * dy + dz * dz;
    float radiusSq = microbeRadius * microbeRadius;
    return distSq <= radiusSq;
}

bool DestructionSystem::applyDamage(flecs::entity microbeEntity, float damage) {
    auto microbe = microbeEntity.get_mut<components::Microbe>();
    if (!microbe) {
        return false;
    }

    microbe->stats.health -= damage;

    if (microbe->stats.health <= 0.0f) {
        return true;  // Microbe destroyed
    }

    return false;
}

void DestructionSystem::destroyMicrobe(flecs::entity microbeEntity, flecs::world& world) {
    auto microbe = microbeEntity.get<components::Microbe>();
    if (!microbe) {
        return;
    }

    auto transform = microbeEntity.get<components::Transform>();
    if (!transform) {
        return;
    }

    // Spawn resources based on microbe type and traits
    // For now, spawn basic resource drops
    // This will be expanded when progression system is implemented

    // Spawn a random resource drop (placeholder)
    // TODO: Determine resource type based on microbe traits
    components::ResourceType resourceType = components::ResourceType::Sodium;
    float resourceAmount = 1.0f + (float)(rand() % 5);  // 1-5 units

    ResourceSystem::spawnResource(world, resourceType, resourceAmount, transform->position);

    // Destroy the entity (FLECS will handle cleanup)
    microbeEntity.destruct();

}

void DestructionSystem::registerSystem(flecs::world& world, PhysicsSystemState* physics) {
    // System that handles hover/click detection and destruction
    // Runs in OnUpdate phase (after Input, before physics)

    // Hover detection system
    world.system<components::Microbe, components::Transform>("DestructionSystem_Hover")
        .kind(flecs::OnUpdate)
        .each([&world, physics](flecs::entity e, components::Microbe& microbe, components::Transform& transform) {
            // Get input state
            auto inputState = world.get<components::InputState>();
            if (!inputState) {
                return;
            }

            // Get mouse ray from camera
            // TODO: Get camera from CameraState singleton
            // For now, use a simple distance check from origin

            // Simple hover detection: check if mouse is near microbe
            // This is a placeholder - proper implementation needs ray casting
            Vector3 mouseWorldPos = {0.0f, 0.0f, 0.0f};  // TODO: Get from input system

            float hoverRadius = microbe.stats.baseRadius * 1.5f;  // Slightly larger than collision radius
            if (isPointInMicrobe(mouseWorldPos, transform.position, hoverRadius)) {
                // Microbe is being hovered - could add visual feedback here
                // For now, just mark it (could add Hovered component)
            }
        });

    // Click destruction system
    world.system<components::Microbe, components::Transform>("DestructionSystem_Click")
        .kind(flecs::OnUpdate)
        .each([&world, physics](flecs::entity e, components::Microbe& microbe, components::Transform& transform) {
            // Get input state
            auto inputState = world.get<components::InputState>();
            if (!inputState) {
                return;
            }

            // Check if mouse was clicked (use mouseLeftPressed, not mouseLeftDown)
            // mouseLeftDown stays true while held, mouseLeftPressed is only true on the frame it's pressed
            if (inputState->mouseLeftPressed) {
                // Simple click detection: check if mouse is near microbe
                // TODO: Proper ray casting from camera
                Vector3 mouseWorldPos = {0.0f, 0.0f, 0.0f};  // TODO: Get from input system

                float clickRadius = microbe.stats.baseRadius * 1.2f;
                if (isPointInMicrobe(mouseWorldPos, transform.position, clickRadius)) {
                    // Apply damage
                    float damage = 100.0f;  // Instant kill for now
                    if (applyDamage(e, damage)) {
                        destroyMicrobe(e, world);
                    }
                }
            }
        });
}

} // namespace micro_idle
