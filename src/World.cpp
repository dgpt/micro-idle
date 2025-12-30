#include "World.h"
#include "components/Transform.h"
#include "components/Physics.h"
#include "components/Rendering.h"
#include "components/Input.h"
#include "components/Microbe.h"
#include "systems/PhysicsSystem.h"
#include "systems/SoftBodyFactory.h"
#include "systems/ECMLocomotionSystem.h"
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

    // Create singleton for input state
    world.set<components::InputState>({});

    // Initialize boundaries
    boundaries = new WorldBoundaries();

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
}

void World::registerSystems() {
    // Render system (PostUpdate phase)
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
    world.each([this, dt](flecs::entity e, components::Microbe& microbe, components::InternalSkeleton& skeleton) {
        ECMLocomotionSystem::update(microbe, skeleton, physics, dt);
    });

    // Update physics
    physics->update(dt);

    // Sync transforms from Jolt to FLECS (simple rigid bodies)
    world.each([this](flecs::entity e, components::PhysicsBody& physBody, components::Transform& transform) {
        if (!physBody.bodyID.IsInvalid()) {
            JPH::BodyInterface& bodyInterface = physics->physicsSystem->GetBodyInterface();
            JPH::RVec3 position = bodyInterface.GetPosition(physBody.bodyID);
            JPH::Quat rotation = bodyInterface.GetRotation(physBody.bodyID);

            transform.position.x = (float)position.GetX();
            transform.position.y = (float)position.GetY();
            transform.position.z = (float)position.GetZ();

            transform.rotation.x = rotation.GetX();
            transform.rotation.y = rotation.GetY();
            transform.rotation.z = rotation.GetZ();
            transform.rotation.w = rotation.GetW();
        }
    });

    // Sync transforms for microbe soft bodies (calculate center from soft body position)
    world.each([this](flecs::entity e, components::Microbe& microbe, components::Transform& transform) {
        if (!microbe.softBody.bodyID.IsInvalid()) {
            JPH::BodyInterface& bodyInterface = physics->physicsSystem->GetBodyInterface();

            // Get soft body center of mass
            JPH::RVec3 pos = bodyInterface.GetCenterOfMassPosition(microbe.softBody.bodyID);
            transform.position.x = (float)pos.GetX();
            transform.position.y = (float)pos.GetY();
            transform.position.z = (float)pos.GetZ();

            // Get rotation (for future use)
            JPH::Quat rot = bodyInterface.GetRotation(microbe.softBody.bodyID);
            transform.rotation.x = rot.GetX();
            transform.rotation.y = rot.GetY();
            transform.rotation.z = rot.GetZ();
            transform.rotation.w = rot.GetW();
        }
    });

    // Progress the world (runs OnUpdate systems)
    world.progress(dt);
}

void World::render(Camera3D camera, float alpha) {
    // Lazy load shader if window is available
    if (sdfMembraneShader.id == 0 && IsWindowReady()) {
        sdfMembraneShader = LoadShader("shaders/sdf_membrane.vert", "shaders/sdf_membrane.frag");
        if (sdfMembraneShader.id == 0) {
            // Try fallback path
            sdfMembraneShader = LoadShader("data/shaders/sdf_membrane.vert", "data/shaders/sdf_membrane.frag");
        }
    }

    // Begin 3D mode
    BeginMode3D(camera);

    // Render simple spheres (petri dish components)
    world.each([](flecs::entity e,
                  components::Transform& transform,
                  components::RenderSphere& sphere,
                  components::RenderColor& color) {
        DrawSphere(transform.position, sphere.radius, color.color);
    });

    // Render microbes using SDF raymarching (smooth membrane over soft body vertices)
    world.each([this, camera](flecs::entity e, components::Microbe& microbe, components::Transform& transform) {
        if (microbe.softBody.vertexCount == 0) return;

        // Extract soft body vertex positions for rendering
        Vector3 vertexPoints[64];
        int count = SoftBodyFactory::ExtractVertexPositions(
            physics,
            microbe.softBody.bodyID,
            vertexPoints,
            64
        );

        if (count == 0) return;

        // Calculate bounding sphere for the microbe
        Vector3 center = transform.position;
        float boundRadius = microbe.stats.baseRadius * 2.5f; // Bounding radius for raymarch volume

        // Skip shader rendering if shader not loaded (for tests without window)
        if (sdfMembraneShader.id == 0) return;

        // Begin shader mode
        BeginShaderMode(sdfMembraneShader);

        // Set shader uniforms
        int viewPosLoc = GetShaderLocation(sdfMembraneShader, "viewPos");
        int pointCountLoc = GetShaderLocation(sdfMembraneShader, "pointCount");
        int baseRadiusLoc = GetShaderLocation(sdfMembraneShader, "baseRadius");
        int colorLoc = GetShaderLocation(sdfMembraneShader, "microbeColor");

        // Upload vertex points array
        for (int i = 0; i < count && i < 64; i++) {
            char uniformName[64];
            snprintf(uniformName, sizeof(uniformName), "skeletonPoints[%d]", i);
            int loc = GetShaderLocation(sdfMembraneShader, uniformName);
            if (loc >= 0) {
                float pos[3] = {vertexPoints[i].x, vertexPoints[i].y, vertexPoints[i].z};
                SetShaderValue(sdfMembraneShader, loc, pos, SHADER_UNIFORM_VEC3);
            }
        }

        SetShaderValue(sdfMembraneShader, viewPosLoc, &camera.position, SHADER_UNIFORM_VEC3);
        SetShaderValue(sdfMembraneShader, pointCountLoc, &count, SHADER_UNIFORM_INT);
        SetShaderValue(sdfMembraneShader, baseRadiusLoc, &microbe.stats.baseRadius, SHADER_UNIFORM_FLOAT);

        Vector3 colorVec = {
            microbe.stats.color.r / 255.0f,
            microbe.stats.color.g / 255.0f,
            microbe.stats.color.b / 255.0f
        };
        SetShaderValue(sdfMembraneShader, colorLoc, &colorVec, SHADER_UNIFORM_VEC3);

        // Draw bounding sphere (the shader will raymarch inside it)
        DrawSphere(center, boundRadius, WHITE);

        EndShaderMode();
    });

    EndMode3D();
}

void World::handleInput(Camera3D camera, float dt, int screen_w, int screen_h) {
    auto input = world.get_mut<components::InputState>();
    if (input) {
        input->mousePosition = GetMousePosition();
        input->mouseDelta = GetMouseDelta();
        input->mouseLeftDown = IsMouseButtonDown(MOUSE_LEFT_BUTTON);
        input->mouseRightDown = IsMouseButtonDown(MOUSE_RIGHT_BUTTON);
        input->mouseWheel = GetMouseWheelMove();
    }
}

void World::renderUI(int screen_w, int screen_h) {
    // Draw simple FPS counter
    DrawText(TextFormat("FPS: %d", GetFPS()), 10, 10, 20, GREEN);

    // Draw entity count
    int entityCount = world.count<components::Transform>();
    DrawText(TextFormat("Entities: %d", entityCount), 10, 35, 20, GREEN);
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
    entity.set<components::Transform>({
        .position = position,
        .rotation = {0.0f, 0.0f, 0.0f, 1.0f},
        .scale = {1.0f, 1.0f, 1.0f}
    });

    // Add microbe to entity
    entity.set<components::Microbe>(microbe);

    printf("Created amoeba entity at (%.1f, %.1f, %.1f)\n", position.x, position.y, position.z);

    return entity;
}

void World::createScreenBoundaries(float worldWidth, float worldHeight) {
    printf("Creating screen boundaries (%.1f x %.1f)\n", worldWidth, worldHeight);

    float wallThickness = 1.0f;
    float wallHeight = 5.0f;

    // Floor
    JPH::Vec3 floorPos(0.0f, 0.0f, 0.0f);
    JPH::Vec3 floorHalfExtents(worldWidth / 2.0f, 0.2f, worldHeight / 2.0f);
    boundaries->floor = physics->createBox(floorPos, floorHalfExtents, true);

    // North wall (+Z)
    JPH::Vec3 northPos(0.0f, wallHeight / 2.0f, worldHeight / 2.0f);
    JPH::Vec3 northHalfExtents(worldWidth / 2.0f, wallHeight / 2.0f, wallThickness / 2.0f);
    boundaries->north = physics->createBox(northPos, northHalfExtents, true);

    // South wall (-Z)
    JPH::Vec3 southPos(0.0f, wallHeight / 2.0f, -worldHeight / 2.0f);
    JPH::Vec3 southHalfExtents(worldWidth / 2.0f, wallHeight / 2.0f, wallThickness / 2.0f);
    boundaries->south = physics->createBox(southPos, southHalfExtents, true);

    // East wall (+X)
    JPH::Vec3 eastPos(worldWidth / 2.0f, wallHeight / 2.0f, 0.0f);
    JPH::Vec3 eastHalfExtents(wallThickness / 2.0f, wallHeight / 2.0f, worldHeight / 2.0f);
    boundaries->east = physics->createBox(eastPos, eastHalfExtents, true);

    // West wall (-X)
    JPH::Vec3 westPos(-worldWidth / 2.0f, wallHeight / 2.0f, 0.0f);
    JPH::Vec3 westHalfExtents(wallThickness / 2.0f, wallHeight / 2.0f, worldHeight / 2.0f);
    boundaries->west = physics->createBox(westPos, westHalfExtents, true);

    printf("Screen boundaries created\n");
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

    // Reposition microbes that are now outside bounds
    repositionMicrobesInBounds(worldWidth, worldHeight);
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
    });
}

} // namespace micro_idle
