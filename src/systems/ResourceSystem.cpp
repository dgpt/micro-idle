#include "ResourceSystem.h"
#include "src/components/Resource.h"
#include "src/components/Transform.h"
#include "src/components/Input.h"
#include "raylib.h"
#include <cstdio>

namespace micro_idle {

flecs::entity ResourceSystem::spawnResource(flecs::world& world,
                                            components::ResourceType type,
                                            float amount,
                                            Vector3 position) {
    auto entity = world.entity();

    // Set resource component
    components::Resource resource;
    resource.type = type;
    resource.amount = amount;
    resource.lifetime = 10.0f;  // 10 seconds default lifetime
    resource.maxLifetime = resource.lifetime;
    resource.isCollected = false;

    // Set color based on resource type
    switch (type) {
        case components::ResourceType::Sodium:
            resource.color = YELLOW;
            break;
        case components::ResourceType::Glucose:
            resource.color = GREEN;
            break;
        case components::ResourceType::Iron:
            resource.color = GRAY;
            break;
        case components::ResourceType::Calcium:
            resource.color = WHITE;
            break;
        case components::ResourceType::Lipids:
            resource.color = ORANGE;
            break;
        case components::ResourceType::Oxygen:
            resource.color = BLUE;
            break;
        case components::ResourceType::SignalingMolecules:
            resource.color = PURPLE;
            break;
    }

    entity.set<components::Resource>(resource);

    // Set transform
    entity.set<components::Transform>({
        .position = position,
        .rotation = {0.0f, 0.0f, 0.0f, 1.0f},
        .scale = {1.0f, 1.0f, 1.0f}
    });

    printf("ResourceSystem: Spawned resource %d at (%.1f, %.1f, %.1f)\n",
           (int)type, position.x, position.y, position.z);

    return entity;
}

void ResourceSystem::collectResource(flecs::entity resourceEntity, flecs::world& world) {
    auto resource = resourceEntity.get_mut<components::Resource>();
    if (!resource || resource->isCollected) {
        return;
    }

    // Get or create resource inventory singleton
    auto inventory = world.get_mut<components::ResourceInventory>();
    if (!inventory) {
        world.set<components::ResourceInventory>({});
        inventory = world.get_mut<components::ResourceInventory>();
    }

    // Add resource to inventory
    inventory->add(resource->type, resource->amount);

    // Mark as collected
    resource->isCollected = true;

    // Destroy the resource entity
    resourceEntity.destruct();

    printf("ResourceSystem: Collected %.2f of resource %d\n",
           resource->amount, (int)resource->type);
}

void ResourceSystem::registerSystem(flecs::world& world) {
    // System that updates resource lifetime and handles collection
    // Runs in OnUpdate phase

    // Lifetime update system
    world.system<components::Resource>("ResourceSystem_Lifetime")
        .kind(flecs::OnUpdate)
        .each([](flecs::entity e, components::Resource& resource) {
            if (resource.isCollected) {
                return;
            }

            // Decrease lifetime (use delta time from iter)
            float dt = 0.016f;  // TODO: Get from iter.delta_time() when available
            resource.lifetime -= dt;

            // Despawn if lifetime expired
            if (resource.lifetime <= 0.0f) {
                e.destruct();
            }
        });

    // Collection system (hover/click to collect)
    world.system<components::Resource, components::Transform>("ResourceSystem_Collection")
        .kind(flecs::OnUpdate)
        .each([&world](flecs::entity e, components::Resource& resource, components::Transform& transform) {
            if (resource.isCollected) {
                return;
            }

            // Get input state
            auto inputState = world.get_mut<components::InputState>();
            if (!inputState) {
                return;
            }

            // Simple collection: click near resource
            // TODO: Proper ray casting from camera
            if (inputState->mouseLeftDown) {
                Vector3 mouseWorldPos = {0.0f, 0.0f, 0.0f};  // TODO: Get from input system

                float collectRadius = 1.0f;  // Collection radius
                float dx = mouseWorldPos.x - transform.position.x;
                float dy = mouseWorldPos.y - transform.position.y;
                float dz = mouseWorldPos.z - transform.position.z;
                float distSq = dx * dx + dy * dy + dz * dz;

                if (distSq <= collectRadius * collectRadius) {
                    collectResource(e, world);
                }
            }
        });
}

} // namespace micro_idle
