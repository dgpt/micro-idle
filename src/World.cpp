#include "World.h"
#include "components/Transform.h"

#include "components/Physics.h"
#include "components/Rendering.h"
#include "components/Input.h"
#include "components/Microbe.h"
#include "systems/PhysicsSystem.h"
#include "systems/SoftBodyFactory.h"
#include "systems/ECMLocomotionSystem.h"
#include "systems/InputSystem.h"
#include "systems/TransformSyncSystem.h"
#include "systems/UpdateSDFUniforms.h"
#include "systems/SDFRenderSystem.h"
#include "systems/SpawnSystem.h"
#include "systems/DestructionSystem.h"
#include "systems/ResourceSystem.h"
#include "components/Resource.h"
#include "components/WorldState.h"
#include "rendering/SDFShader.h"
#include "rendering/RaymarchBounds.h"
#include <stdio.h>
#include <cmath>
#include <algorithm>

namespace micro_idle {

// Opaque struct to hold boundary BodyIDs without exposing Jolt headers in World.h
struct WorldBoundaries {
    JPH::BodyID north;
    JPH::BodyID south;
    JPH::BodyID east;
    JPH::BodyID west;
    JPH::BodyID floor;

    WorldBoundaries() :
        north(), south(), east(), west(), floor() {}
};

World::World() {
    // Initialize Jolt physics
    physics = new PhysicsSystemState();

    // Shader will be loaded lazily in render() when window is available
    sdfMembraneShader.id = 0;

    // Create render texture lazily when a window/context exists
    renderTexture.id = 0;

    // Register components and systems
    registerComponents();
    registerSystems();
    registerPhysicsObservers();

    // Create singletons
    world.set<components::InputState>({});
    world.set<components::CameraState>({});
    world.set<components::ResourceInventory>({});
    world.set<components::WorldState>({});

    // Initialize boundaries
    boundaries = new WorldBoundaries();

    // Initialize spawn queue
    spawnQueue = std::vector<SpawnRequest>();

}

World::~World() {

    // Clean up boundaries
    if (boundaries) {
        if (!boundaries->north.IsInvalid()) physics->destroyBody(boundaries->north);
        if (!boundaries->south.IsInvalid()) physics->destroyBody(boundaries->south);
        if (!boundaries->east.IsInvalid()) physics->destroyBody(boundaries->east);
        if (!boundaries->west.IsInvalid()) physics->destroyBody(boundaries->west);
        if (!boundaries->floor.IsInvalid()) physics->destroyBody(boundaries->floor);
        delete boundaries;
    }

    if (renderTexture.id != 0) {
        UnloadRenderTexture(renderTexture);
    }

    // Only unload shader if it was loaded
    if (sdfMembraneShader.id != 0) {
        UnloadShader(sdfMembraneShader);
    }
    delete physics;
}

void World::registerComponents() {
    // Register all components with FLECS
    world.component<components::Transform>();
    world.component<components::PhysicsBody>();
    world.component<components::RenderMesh>();
    world.component<components::RenderColor>();
    world.component<components::InputState>();
    world.component<components::Microbe>();
    world.component<components::ECMLocomotion>();
    world.component<components::InternalSkeleton>();
    world.component<components::SDFRenderComponent>();
    world.component<components::CameraState>();
    world.component<components::Resource>();
    world.component<components::ResourceInventory>();
    world.component<components::WorldState>();
}

void World::registerSystems() {
    // Register systems in proper phase order:
    // 1. InputSystem (OnUpdate - first phase)
    InputSystem::registerSystem(world);

    // 2. ECMLocomotionSystem (OnUpdate - apply forces to soft bodies)
    ECMLocomotionSystem::registerSystem(world, physics);

    // 3. TransformSyncSystem (OnStore - after physics, before render)
    TransformSyncSystem::registerSystem(world, physics);

    // 3. UpdateSDFUniforms (OnStore - after TransformSync)
    // Temporarily disable expensive SDF uniform updates for performance testing
    UpdateSDFUniforms::registerSystem(world, physics);

    // 4. SpawnSystem (OnUpdate - spawn microbes)
    SpawnSystem::registerSystem(world, this);

    // 5. DestructionSystem (OnUpdate - hover/click detection)
    DestructionSystem::registerSystem(world, physics);

    // 6. ResourceSystem (OnUpdate - resource lifetime and collection)
    ResourceSystem::registerSystem(world);

    // 7. SDFRenderSystem (PostUpdate - render pipeline)
    SDFRenderSystem::registerSystem(world);

    // Pipelines: split update and render so PostUpdate only runs during render()
    onUpdatePipeline = world.pipeline()
        .with(flecs::System)
        .with(flecs::OnUpdate)
        .without(flecs::Disabled)
        .build();

    onStorePipeline = world.pipeline()
        .with(flecs::System)
        .with(flecs::OnStore)
        .without(flecs::Disabled)
        .build();

    postUpdatePipeline = world.pipeline()
        .with(flecs::System)
        .with(flecs::PostUpdate)
        .without(flecs::Disabled)
        .build();
}

void World::registerPhysicsObservers() {
    // Observer: When PhysicsBody component is added, create Jolt body
    world.observer<components::PhysicsBody, components::Transform>()
        .event(flecs::OnSet)
        .each([this](flecs::entity e, components::PhysicsBody& physBody, components::Transform& transform) {
            if (physBody.bodyID.IsInvalid()) {
                // Create Jolt sphere body - use default radius for now
                JPH::Vec3 pos(transform.position.x, transform.position.y, transform.position.z);
                float defaultRadius = 1.0f; // Default radius since we don't have RenderSphere anymore
                physBody.bodyID = physics->createSphere(pos, defaultRadius, physBody.isStatic);
            }
        });

    // Observer: When PhysicsBody component is removed, destroy Jolt body
    world.observer<components::PhysicsBody>()
        .event(flecs::OnRemove)
        .each([this](flecs::entity e, components::PhysicsBody& physBody) {
            if (!physBody.bodyID.IsInvalid()) {
                physics->destroyBody(physBody.bodyID);
            }
        });
}

void World::update(float dt) {

    // Update physics (runs in OnUpdate phase via system)
    physics->update(dt);

    // Progress the world (runs OnUpdate systems, then OnStore systems)
    // OnUpdate: InputSystem, EC&M locomotion (above), physics (above)
    // OnStore: TransformSyncSystem, UpdateSDFUniforms
    if (onUpdatePipeline.is_valid()) {
        world.run_pipeline(onUpdatePipeline, dt);
    }
    if (onStorePipeline.is_valid()) {
        world.run_pipeline(onStorePipeline, dt);
    }

    // Execute deferred spawns (after progress to avoid readonly issues)
    for (const auto& request : spawnQueue) {
        auto entity = createAmoeba(request.position, request.radius, request.color);
    }
    spawnQueue.clear();
}

void World::render(Camera3D camera, float alpha, bool renderToTexture) {
    (void)alpha;
    (void)renderToTexture;

    // Update camera state singleton for rendering systems
    auto cameraState = world.get_mut<components::CameraState>();
    if (cameraState) {
        cameraState->position = camera.position;
        cameraState->target = camera.target;
        cameraState->up = camera.up;
        cameraState->fovy = camera.fovy;
    }

    // Lazy load shaders for microbes (create SDFRenderComponent if needed)
    if (sdfMembraneShader.id == 0 && IsWindowReady()) {
        sdfMembraneShader = rendering::loadSDFMembraneShader();
    }

    // Assign shader to all microbes that don't have it yet (or have shader.id=0)
    if (sdfMembraneShader.id != 0) {
        world.each([this](flecs::entity e, components::Microbe& microbe) {
            auto sdf = e.get_mut<components::SDFRenderComponent>();
            if (!sdf) {
                // Create SDFRenderComponent if it doesn't exist
                components::SDFRenderComponent newSdf;
                newSdf.shader = sdfMembraneShader;
                e.set<components::SDFRenderComponent>(newSdf);
            } else if (sdf->shader.id == 0) {
                // Assign shader if component exists but shader isn't loaded
                sdf->shader = sdfMembraneShader;
            }
        });
    }
    // Run render systems inside a 3D context
    BeginMode3D(camera);
    if (postUpdatePipeline.is_valid()) {
        world.run_pipeline(postUpdatePipeline, 0.0f);
    }
    EndMode3D();
}

void World::handleInput(Camera3D camera, float dt, int screen_w, int screen_h) {
    (void)dt;
    (void)screen_w;
    (void)screen_h;

    auto input = world.get_mut<components::InputState>();
    if (!input) {
        return;
    }

    Vector2 mouse = GetMousePosition();
    Ray ray = GetMouseRay(mouse, camera);
    float denom = ray.direction.y;
    if (fabsf(denom) > 0.0001f) {
        float t = -ray.position.y / denom;
        if (t >= 0.0f) {
            input->mouseWorld = {
                ray.position.x + ray.direction.x * t,
                0.0f,
                ray.position.z + ray.direction.z * t
            };
            input->mouseWorldValid = true;
            return;
        }
    }
    input->mouseWorldValid = false;
}

void World::renderUI(int screen_w, int screen_h) {
    (void)screen_w;
    (void)screen_h;
}

flecs::entity World::createTestSphere(Vector3 position, float radius, Color color, bool withPhysics, bool isStatic) {
    auto entity = world.entity();

    entity.set<components::Transform>({
        .position = position,
        .rotation = {0.0f, 0.0f, 0.0f, 1.0f},
        .scale = {1.0f, 1.0f, 1.0f}
    });

    entity.set<components::RenderColor>({
        .color = color
    });

    if (withPhysics) {
        entity.set<components::PhysicsBody>({
            .bodyID = JPH::BodyID(),
            .mass = 1.0f,
            .isStatic = isStatic
        });
    }

    return entity;
}

flecs::entity World::createAmoeba(Vector3 position, float radius, Color color) {

    auto entity = world.entity();

    // Create microbe component with unique seed
    components::Microbe microbe;
    microbe.type = components::MicrobeType::Amoeba;
    microbe.stats.seed = (float)rand() / RAND_MAX;  // Unique seed for each amoeba
    microbe.stats.baseRadius = radius;
    microbe.stats.color = color;
    microbe.stats.health = 100.0f;
    microbe.stats.energy = 100.0f;

    // Create Jolt soft body using Puppet architecture with internal skeleton (Internal Motor model)
    int subdivisions = 1;  // 42 vertices (balanced detail vs. performance)
    std::vector<JPH::BodyID> skeletonBodyIDs;
    microbe.softBody.bodyID = SoftBodyFactory::CreateAmoeba(physics, position, radius, subdivisions, skeletonBodyIDs);
    microbe.softBody.vertexCount = SoftBodyFactory::GetVertexCount(physics, microbe.softBody.bodyID);
    microbe.softBody.subdivisions = subdivisions;


    // Create and set InternalSkeleton component
    components::InternalSkeleton skeleton;
    skeleton.skeletonBodyIDs = skeletonBodyIDs;
    skeleton.skeletonNodeCount = (int)skeletonBodyIDs.size();
    entity.set<components::InternalSkeleton>(skeleton);

    components::ECMLocomotion locomotion;
    ECMLocomotionSystem::initialize(locomotion, microbe.stats.seed);

    // Set transform to initial position
    entity.set<components::Transform>({
        .position = position,
        .rotation = {0.0f, 0.0f, 0.0f, 1.0f},
        .scale = {1.0f, 1.0f, 1.0f}
    });

    // Add microbe to entity
    entity.set<components::Microbe>(microbe);

    // Add EC&M locomotion component
    entity.set<components::ECMLocomotion>(locomotion);

    // Create SDF render component (shader will be loaded lazily in render())
    components::SDFRenderComponent sdf;
    sdf.shader.id = 0; // Will be set when shader is loaded
    entity.set<components::SDFRenderComponent>(sdf);


    return entity;
}

void World::createScreenBoundaries(float worldWidth, float worldHeight) {

    // Update world state singleton
    auto worldState = world.get_mut<components::WorldState>();
    if (worldState) {
        worldState->worldWidth = worldWidth;
        worldState->worldHeight = worldHeight;
    }

    float wallThickness = 1.0f;
    float wallHeight = 10.0f;  // Tall enough to contain falling microbes

    // Floor at y=0 (ground level)
    JPH::Vec3 floorPos(0.0f, 0.0f, 0.0f);
    JPH::Vec3 floorHalfExtents(worldWidth / 2.0f, 0.2f, worldHeight / 2.0f);
    boundaries->floor = physics->createBox(floorPos, floorHalfExtents, true);

    // North wall (+Z) - positioned at worldHeight/2
    JPH::Vec3 northPos(0.0f, wallHeight / 2.0f, worldHeight / 2.0f);
    JPH::Vec3 northHalfExtents(worldWidth / 2.0f, wallHeight / 2.0f, wallThickness / 2.0f);
    boundaries->north = physics->createBox(northPos, northHalfExtents, true);

    // South wall (-Z) - positioned at -worldHeight/2
    JPH::Vec3 southPos(0.0f, wallHeight / 2.0f, -worldHeight / 2.0f);
    JPH::Vec3 southHalfExtents(worldWidth / 2.0f, wallHeight / 2.0f, wallThickness / 2.0f);
    boundaries->south = physics->createBox(southPos, southHalfExtents, true);

    // East wall (+X) - positioned at worldWidth/2
    JPH::Vec3 eastPos(worldWidth / 2.0f, wallHeight / 2.0f, 0.0f);
    JPH::Vec3 eastHalfExtents(wallThickness / 2.0f, wallHeight / 2.0f, worldHeight / 2.0f);
    boundaries->east = physics->createBox(eastPos, eastHalfExtents, true);

    // West wall (-X) - positioned at -worldWidth/2
    JPH::Vec3 westPos(-worldWidth / 2.0f, wallHeight / 2.0f, 0.0f);
    JPH::Vec3 westHalfExtents(wallThickness / 2.0f, wallHeight / 2.0f, worldHeight / 2.0f);
    boundaries->west = physics->createBox(westPos, westHalfExtents, true);
}

void World::updateScreenBoundaries(float worldWidth, float worldHeight) {

    // Destroy old boundaries
    if (!boundaries->north.IsInvalid()) physics->destroyBody(boundaries->north);
    if (!boundaries->south.IsInvalid()) physics->destroyBody(boundaries->south);
    if (!boundaries->east.IsInvalid()) physics->destroyBody(boundaries->east);
    if (!boundaries->west.IsInvalid()) physics->destroyBody(boundaries->west);
    if (!boundaries->floor.IsInvalid()) physics->destroyBody(boundaries->floor);

    // Create new boundaries with updated size
    createScreenBoundaries(worldWidth, worldHeight);

    // Only reposition microbes that are significantly outside bounds (not on every boundary update)
    // This prevents microbes from being constantly repositioned
    // repositionMicrobesInBounds(worldWidth, worldHeight);  // Disabled - only call explicitly when needed
}

void World::repositionMicrobesInBounds(float worldWidth, float worldHeight) {
    // Find all microbes and check if they're outside the new bounds
    world.each([this, worldWidth, worldHeight](flecs::entity e, components::Microbe& microbe, components::Transform& transform) {
        float halfWidth = worldWidth / 2.0f - 2.0f;  // Leave margin for microbe radius
        float halfHeight = worldHeight / 2.0f - 2.0f;

        bool needsReposition = false;
        Vector3 newPos = transform.position;

        // Check X bounds
        if (transform.position.x > halfWidth || transform.position.x < -halfWidth) {
            // Keep X position if within bounds, otherwise clamp to edge
            newPos.x = fmaxf(-halfWidth, fminf(halfWidth, transform.position.x));
            needsReposition = true;
        }

        // Check Z bounds
        if (transform.position.z > halfHeight || transform.position.z < -halfHeight) {
            // Keep Z position if within bounds, otherwise clamp to edge
            newPos.z = fmaxf(-halfHeight, fminf(halfHeight, transform.position.z));
            needsReposition = true;
        }

        if (needsReposition) {
            // Only reposition if microbe is significantly outside bounds
            // Don't reposition if it's just slightly outside (allow some margin)
            float margin = 5.0f;  // Allow 5 units margin before repositioning
            if (fabsf(transform.position.x) > halfWidth + margin ||
                fabsf(transform.position.z) > halfHeight + margin) {
                // Drop from above like "falling from the heavens"
                newPos.y = 25.0f;  // High above camera (camera is at y=22)

                // Update transform
                transform.position = newPos;

                // Update soft body position in physics and reset velocity
                if (!microbe.softBody.bodyID.IsInvalid()) {
                    JPH::BodyInterface& bodyInterface = physics->physicsSystem->GetBodyInterface();
                    bodyInterface.SetPosition(microbe.softBody.bodyID,
                                             JPH::RVec3(newPos.x, newPos.y, newPos.z),
                                             JPH::EActivation::Activate);

                    // Reset velocity so they fall cleanly
                    bodyInterface.SetLinearVelocity(microbe.softBody.bodyID, JPH::Vec3::sZero());
                    bodyInterface.SetAngularVelocity(microbe.softBody.bodyID, JPH::Vec3::sZero());
                }
            }
        }
    });
}

} // namespace micro_idle
