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
#include "rlgl.h"
#include <stdio.h>
#include <cmath>
#include <cstdint>

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
    printf("FLECS World: Initializing...\n");
    fflush(stdout);

    // Initialize Jolt physics
    physics = new PhysicsSystemState();

    // Shader will be loaded lazily in render() when window is available
    sdfMembraneShader.id = 0;

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

    printf("FLECS World: Ready\n");
    fflush(stdout);
}

World::~World() {
    printf("FLECS World: Shutting down\n");

    // Clean up boundaries
    if (boundaries) {
        if (!boundaries->north.IsInvalid()) physics->destroyBody(boundaries->north);
        if (!boundaries->south.IsInvalid()) physics->destroyBody(boundaries->south);
        if (!boundaries->east.IsInvalid()) physics->destroyBody(boundaries->east);
        if (!boundaries->west.IsInvalid()) physics->destroyBody(boundaries->west);
        if (!boundaries->floor.IsInvalid()) physics->destroyBody(boundaries->floor);
        delete boundaries;
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
    world.component<components::RenderSphere>();
    world.component<components::InputState>();
    world.component<components::Microbe>();
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

    // 2. TransformSyncSystem (OnStore - after physics, before render)
    TransformSyncSystem::registerSystem(world, physics);

    // 3. UpdateSDFUniforms (OnStore - after TransformSync)
    UpdateSDFUniforms::registerSystem(world, physics);

    // 4. SpawnSystem (OnUpdate - spawn microbes)
    SpawnSystem::registerSystem(world, this);

    // 5. DestructionSystem (OnUpdate - hover/click detection)
    DestructionSystem::registerSystem(world, physics);

    // 6. ResourceSystem (OnUpdate - resource lifetime and collection)
    ResourceSystem::registerSystem(world);

    // 7. SDFRenderSystem (PostUpdate - final phase)
    SDFRenderSystem::registerSystem(world);

    // Simple sphere render system (PostUpdate phase)
    world.system<const components::Transform, const components::RenderSphere, const components::RenderColor>()
        .kind(flecs::PostUpdate)
        .each([](flecs::entity e,
                const components::Transform& transform,
                const components::RenderSphere& sphere,
                const components::RenderColor& color) {
            DrawSphere(transform.position, sphere.radius, color.color);
        });
}

void World::registerPhysicsObservers() {
    // Observer: When PhysicsBody component is added, create Jolt body
    world.observer<components::PhysicsBody, components::Transform, components::RenderSphere>()
        .event(flecs::OnSet)
        .each([this](flecs::entity e, components::PhysicsBody& physBody, components::Transform& transform, components::RenderSphere& sphere) {
            if (physBody.bodyID.IsInvalid()) {
                // Create Jolt sphere body
                JPH::Vec3 pos(transform.position.x, transform.position.y, transform.position.z);
                physBody.bodyID = physics->createSphere(pos, sphere.radius, physBody.isStatic);
                printf("Physics: Created body for entity %llu (static=%d)\n", e.id(), physBody.isStatic);
            }
        });

    // Observer: When PhysicsBody component is removed, destroy Jolt body
    world.observer<components::PhysicsBody>()
        .event(flecs::OnRemove)
        .each([this](flecs::entity e, components::PhysicsBody& physBody) {
            if (!physBody.bodyID.IsInvalid()) {
                physics->destroyBody(physBody.bodyID);
                printf("Physics: Destroyed body for entity %llu\n", e.id());
            }
        });
}

void World::update(float dt) {
    // Update EC&M locomotion for all microbes (apply forces to internal skeleton)
    // This runs before physics update in OnUpdate phase
    world.each([this, dt](flecs::entity e, components::Microbe& microbe, components::InternalSkeleton& skeleton) {
        ECMLocomotionSystem::update(microbe, skeleton, physics, dt);
    });

    // Update physics (runs in OnUpdate phase via system)
    physics->update(dt);

    // Progress the world (runs OnUpdate systems, then OnStore systems)
    // OnUpdate: InputSystem, EC&M locomotion (above), physics (above)
    // OnStore: TransformSyncSystem, UpdateSDFUniforms
    world.progress(dt);

    // Execute deferred spawns (after progress to avoid readonly issues)
    for (const auto& request : spawnQueue) {
        auto entity = createAmoeba(request.position, request.radius, request.color);
        printf("World: Executed deferred spawn - entity %llu at (%.1f, %.1f, %.1f)\n",
               entity.id(), request.position.x, request.position.y, request.position.z);
    }
    spawnQueue.clear();
}

void World::render(Camera3D camera, float alpha, bool renderToTexture) {
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

    // Count microbes before rendering (disabled logging for performance)
    // static int lastMicrobeCount = -1;
    // int microbeCount = world.count<components::Microbe>();
    // if (microbeCount != lastMicrobeCount) {
    //     printf("World::render() - Microbe count: %d\n", microbeCount);
    //     lastMicrobeCount = microbeCount;
    // }

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

    // Set camera position uniform for all SDF shaders (needed for raymarching)
    if (sdfMembraneShader.id != 0) {
        rendering::SDFShaderUniforms uniforms;
        if (rendering::initializeSDFUniforms(sdfMembraneShader, uniforms)) {
            rendering::setCameraPosition(sdfMembraneShader, uniforms, camera.position);
        }
    }

    // Begin 3D mode (skip if rendering to texture - matrices already set up)
    if (!renderToTexture) {
        BeginMode3D(camera);
    }

    // Render simple spheres (petri dish components)
    world.each([](flecs::entity e,
                  const components::Transform& transform,
                  const components::RenderSphere& sphere,
                  const components::RenderColor& color) {
        DrawSphere(transform.position, sphere.radius, color.color);
    });

    // Manually render microbes via SDF
    // Note: PostUpdate systems would run here, but we need to render in 3D mode
    // So we render manually to ensure it happens at the right time
    world.each([this, camera](flecs::entity e, const components::Microbe& microbe, const components::Transform& transform) {
        auto sdf = e.get<components::SDFRenderComponent>();
        if (!sdf) {
            return;
        }

        // Skip if no vertices or shader not loaded
        if (sdf->vertexCount == 0 || sdf->shader.id == 0) {
            return;
        }

        // Initialize shader uniforms if not already done
        if (!sdf->shaderLoaded) {
            rendering::SDFShaderUniforms uniforms;
            if (rendering::initializeSDFUniforms(sdf->shader, uniforms)) {
                auto sdfMut = e.get_mut<components::SDFRenderComponent>();
                sdfMut->shaderLocViewPos = uniforms.viewPos;
                sdfMut->shaderLocPointCount = uniforms.pointCount;
                sdfMut->shaderLocBaseRadius = uniforms.baseRadius;
                sdfMut->shaderLocMicrobeColor = uniforms.microbeColor;
                for (int i = 0; i < 64; i++) {
                    sdfMut->shaderLocSkeletonPoints[i] = uniforms.skeletonPoints[i];
                }
                sdfMut->shaderLoaded = true;
            } else {
                return;  // Failed to initialize uniforms
            }
        }

        // Render the microbe
        printf("RENDERING MICROBE: position=(%.2f,%.2f,%.2f), radius=%.2f\n",
               transform.position.x, transform.position.y, transform.position.z,
               microbe.stats.baseRadius);

        Vector3 center = transform.position;
        float boundRadius = rendering::calculateBoundRadius(microbe.stats.baseRadius);

        BeginShaderMode(sdf->shader);

        // Set uniforms and vertex positions for this specific microbe
        rendering::SDFShaderUniforms uniforms;
        uniforms.viewPos = sdf->shaderLocViewPos;
        uniforms.pointCount = sdf->shaderLocPointCount;
        uniforms.baseRadius = sdf->shaderLocBaseRadius;
        uniforms.microbeColor = sdf->shaderLocMicrobeColor;
        for (int i = 0; i < 64; i++) {
            uniforms.skeletonPoints[i] = sdf->shaderLocSkeletonPoints[i];
        }

        // Set microbe-specific uniforms
        rendering::setMicrobeUniforms(sdf->shader, uniforms, sdf->vertexCount,
                                     microbe.stats.baseRadius, microbe.stats.color);

        // Set vertex positions for this microbe
        rendering::setVertexPositions(sdf->shader, uniforms, sdf->vertexPositions, sdf->vertexCount);

        DrawSphere(center, boundRadius, WHITE);
        EndShaderMode();
    });

    // End 3D mode (skip if rendering to texture)
    if (!renderToTexture) {
        EndMode3D();
    }
}

void World::handleInput(Camera3D camera, float dt, int screen_w, int screen_h) {
    // Input is now handled by InputSystem in OnUpdate phase
    // This method is kept for compatibility but does nothing
    // InputSystem polls Raylib and updates InputState singleton automatically
    (void)camera;
    (void)dt;
    (void)screen_w;
    (void)screen_h;
}

void World::renderUI(int screen_w, int screen_h) {
    // Draw simple FPS counter
    DrawText(TextFormat("FPS: %d", GetFPS()), 10, 10, 20, GREEN);

    // Draw entity count
    int entityCount = world.count<components::Transform>();
    int microbeCount = world.count<components::Microbe>();
    DrawText(TextFormat("Entities: %d | Microbes: %d", entityCount, microbeCount), 10, 35, 20, GREEN);
}

flecs::entity World::createTestSphere(Vector3 position, float radius, Color color, bool withPhysics, bool isStatic) {
    auto entity = world.entity();

    entity.set<components::Transform>({
        .position = position,
        .rotation = {0.0f, 0.0f, 0.0f, 1.0f},
        .scale = {1.0f, 1.0f, 1.0f}
    });

    entity.set<components::RenderSphere>({
        .radius = radius
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
    printf("createAmoeba: Called with position=(%.2f, %.2f, %.2f), radius=%.2f\n",
           position.x, position.y, position.z, radius);

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
    int subdivisions = 1;  // 42 vertices (good balance for amoeba)
    std::vector<JPH::BodyID> skeletonBodyIDs;
    microbe.softBody.bodyID = SoftBodyFactory::CreateAmoeba(physics, position, radius, subdivisions, skeletonBodyIDs);
    microbe.softBody.vertexCount = SoftBodyFactory::GetVertexCount(physics, microbe.softBody.bodyID);
    microbe.softBody.subdivisions = subdivisions;

    printf("  Created soft body with %d vertices\n", microbe.softBody.vertexCount);

    // Create and set InternalSkeleton component
    components::InternalSkeleton skeleton;
    skeleton.skeletonBodyIDs = skeletonBodyIDs;
    skeleton.skeletonNodeCount = (int)skeletonBodyIDs.size();
    entity.set<components::InternalSkeleton>(skeleton);

    // Initialize EC&M locomotion with unique random values based on microbe's seed
    auto hashFloat = [](float seed, int iter) -> float {
        uint32_t hash = (uint32_t)(seed * 1000000.0f);
        hash = hash * 2654435761u + (uint32_t)iter;
        hash = (hash ^ (hash >> 16)) * 0x85ebca6b;
        hash = (hash ^ (hash >> 13)) * 0xc2b2ae35;
        hash = hash ^ (hash >> 16);
        return (float)(hash % 10000) / 10000.0f;
    };

    microbe.locomotion.phase = hashFloat(microbe.stats.seed, 0);
    microbe.locomotion.targetVertexIndex = 0;  // Target a soft body vertex
    float angle = hashFloat(microbe.stats.seed, 1) * 2.0f * PI;
    microbe.locomotion.targetDirection = {cosf(angle), 0.0f, sinf(angle)};
    microbe.locomotion.wigglePhase = hashFloat(microbe.stats.seed, 2) * 2.0f * PI;

    // Set phase flags
    microbe.locomotion.isExtending = (microbe.locomotion.phase < ECMLocomotionSystem::EXTEND_PHASE);
    microbe.locomotion.isSearching = (microbe.locomotion.phase >= ECMLocomotionSystem::EXTEND_PHASE &&
                                      microbe.locomotion.phase < ECMLocomotionSystem::SEARCH_PHASE);
    microbe.locomotion.isRetracting = (microbe.locomotion.phase >= ECMLocomotionSystem::SEARCH_PHASE);

    // Set transform to initial position
    printf("createAmoeba: Setting Transform to (%.2f, %.2f, %.2f)\n", position.x, position.y, position.z);
    entity.set<components::Transform>({
        .position = position,
        .rotation = {0.0f, 0.0f, 0.0f, 1.0f},
        .scale = {1.0f, 1.0f, 1.0f}
    });

    // Add microbe to entity
    entity.set<components::Microbe>(microbe);

    // Create SDF render component (shader will be loaded lazily in render())
    components::SDFRenderComponent sdf;
    sdf.shader.id = 0; // Will be set when shader is loaded
    sdf.shaderLoaded = false;
    entity.set<components::SDFRenderComponent>(sdf);

    printf("Created amoeba entity at (%.1f, %.1f, %.1f)\n", position.x, position.y, position.z);

    return entity;
}

void World::createScreenBoundaries(float worldWidth, float worldHeight) {
    printf("Creating screen boundaries (%.1f x %.1f)\n", worldWidth, worldHeight);

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

    printf("Screen boundaries created: floor at y=0, walls at x=±%.1f, z=±%.1f\n",
           worldWidth / 2.0f, worldHeight / 2.0f);
}

void World::updateScreenBoundaries(float worldWidth, float worldHeight) {
    printf("Updating screen boundaries to %.1f x %.1f\n", worldWidth, worldHeight);

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

                printf("Repositioning microbe - dropping from above to (%.1f, %.1f, %.1f)\n",
                       newPos.x, newPos.y, newPos.z);

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
