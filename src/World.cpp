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

namespace micro_idle {

World::World() : shadersLoaded(false) {
    printf("FLECS World: Initializing...\n");
    fflush(stdout);

    // Initialize Jolt physics
    physics = new PhysicsSystemState();

    // Register components and systems
    registerComponents();
    registerSystems();
    registerPhysicsObservers();

    // Create singleton for input state
    world.set<components::InputState>({});

    // Load metaball shaders
    loadMetaballShaders();

    printf("FLECS World: Ready\n");
    fflush(stdout);
}

World::~World() {
    printf("FLECS World: Shutting down\n");
    if (shadersLoaded) {
        UnloadShader(metaballShader);
        UnloadMesh(billboardQuad);
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
    // Update EC&M locomotion for all microbes
    world.each([this, dt](flecs::entity e, components::Microbe& microbe) {
        ECMLocomotionSystem::update(microbe, physics, dt);
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

    // Sync transforms for microbe soft bodies (calculate center of mass)
    world.each([this](flecs::entity e, components::Microbe& microbe, components::Transform& transform) {
        if (!microbe.softBody.particleBodyIDs.empty()) {
            JPH::BodyInterface& bodyInterface = physics->physicsSystem->GetBodyInterface();

            // Calculate center of mass from all particles
            Vector3 center = {0, 0, 0};
            for (JPH::BodyID bodyID : microbe.softBody.particleBodyIDs) {
                JPH::RVec3 pos = bodyInterface.GetPosition(bodyID);
                center.x += (float)pos.GetX();
                center.y += (float)pos.GetY();
                center.z += (float)pos.GetZ();
            }
            int count = (int)microbe.softBody.particleBodyIDs.size();
            if (count > 0) {
                transform.position.x = center.x / count;
                transform.position.y = center.y / count;
                transform.position.z = center.z / count;
            }
        }
    });

    // Progress the world (runs OnUpdate systems)
    world.progress(dt);
}

void World::render(Camera3D camera, float alpha) {
    // Begin 3D mode
    BeginMode3D(camera);

    // Render simple spheres (petri dish components)
    world.each([](flecs::entity e,
                  components::Transform& transform,
                  components::RenderSphere& sphere,
                  components::RenderColor& color) {
        DrawSphere(transform.position, sphere.radius, color.color);
    });

    // Render microbes as mesh surface
    world.each([this](flecs::entity e, components::Microbe& microbe) {
        if (microbe.softBody.membrane.meshVertexCount == 0) return;

        JPH::BodyInterface& bodyInterface = physics->physicsSystem->GetBodyInterface();
        int meshStart = microbe.softBody.membrane.meshVertexStartIndex;

        // Draw mesh triangles
        for (size_t i = 0; i + 2 < microbe.softBody.membrane.triangleIndices.size(); i += 3) {
            int idx0 = meshStart + microbe.softBody.membrane.triangleIndices[i];
            int idx1 = meshStart + microbe.softBody.membrane.triangleIndices[i + 1];
            int idx2 = meshStart + microbe.softBody.membrane.triangleIndices[i + 2];

            // Bounds check
            if (idx0 >= (int)microbe.softBody.particleBodyIDs.size() ||
                idx1 >= (int)microbe.softBody.particleBodyIDs.size() ||
                idx2 >= (int)microbe.softBody.particleBodyIDs.size()) {
                continue;
            }

            JPH::RVec3 p0 = bodyInterface.GetPosition(microbe.softBody.particleBodyIDs[idx0]);
            JPH::RVec3 p1 = bodyInterface.GetPosition(microbe.softBody.particleBodyIDs[idx1]);
            JPH::RVec3 p2 = bodyInterface.GetPosition(microbe.softBody.particleBodyIDs[idx2]);

            Vector3 v0 = {(float)p0.GetX(), (float)p0.GetY(), (float)p0.GetZ()};
            Vector3 v1 = {(float)p1.GetX(), (float)p1.GetY(), (float)p1.GetZ()};
            Vector3 v2 = {(float)p2.GetX(), (float)p2.GetY(), (float)p2.GetZ()};

            DrawTriangle3D(v0, v1, v2, microbe.stats.color);
        }
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

    // Create microbe component
    components::Microbe microbe;
    microbe.type = components::MicrobeType::Amoeba;
    microbe.stats.seed = (float)rand() / RAND_MAX;
    microbe.stats.baseRadius = radius;
    microbe.stats.color = color;
    microbe.stats.health = 100.0f;
    microbe.stats.energy = 100.0f;

    // Create soft body
    microbe.softBody = SoftBodyFactory::createAmoeba(physics, position, radius, 16);

    // Initialize EC&M locomotion
    ECMLocomotionSystem::initialize(microbe.locomotion);

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

void World::createPetriDish(Vector3 position, float radius, float edgeHeight) {
    printf("Creating petri dish (radius=%.1f, edge height=%.1f)\n", radius, edgeHeight);

    // Create flat circular disc (bottom of petri dish)
    JPH::Vec3 discPos(position.x, position.y, position.z);
    physics->createCylinder(discPos, radius, 0.2f, true); // Thin disc

    // Create raised circular edge using multiple boxes arranged in a circle
    int segmentCount = 32; // Number of segments for smooth circle
    float segmentAngle = 2.0f * PI / segmentCount;
    float segmentWidth = 2.0f * PI * radius / segmentCount; // Arc length

    for (int i = 0; i < segmentCount; i++) {
        float angle = i * segmentAngle;
        float x = position.x + radius * cosf(angle);
        float z = position.z + radius * sinf(angle);
        float y = position.y + edgeHeight / 2.0f;

        JPH::Vec3 segmentPos(x, y, z);
        JPH::Vec3 halfExtents(segmentWidth / 2.0f, edgeHeight / 2.0f, 0.15f); // Thin wall
        physics->createBox(segmentPos, halfExtents, true);
    }

    printf("Petri dish created with %d edge segments\n", segmentCount);
}

void World::loadMetaballShaders() {
    // Load custom metaball shaders
    const char* vsPath = "data/shaders/metaball.vert";
    const char* fsPath = "data/shaders/metaball.frag";

    if (FileExists(vsPath) && FileExists(fsPath)) {
        metaballShader = LoadShader(vsPath, fsPath);
        if (metaballShader.id > 0) {
            printf("Metaball shaders loaded successfully\n");

            // Create billboard quad mesh (will be drawn for each particle)
            billboardQuad = GenMeshPlane(2.0f, 2.0f, 1, 1);

            shadersLoaded = true;
        } else {
            printf("Warning: Failed to load metaball shaders\n");
            shadersLoaded = false;
        }
    } else {
        printf("Warning: Metaball shader files not found, using fallback rendering\n");
        shadersLoaded = false;
    }
}

void World::renderMicrobeParticles(const components::Microbe& microbe, Camera3D camera) {
    if (microbe.softBody.particleBodyIDs.empty()) return;

    JPH::BodyInterface& bodyInterface = physics->physicsSystem->GetBodyInterface();

    // Calculate camera vectors for billboarding
    Vector3 cameraForward = Vector3Subtract(camera.target, camera.position);
    cameraForward = Vector3Normalize(cameraForward);
    Vector3 cameraRight = Vector3CrossProduct(camera.up, cameraForward);
    cameraRight = Vector3Normalize(cameraRight);
    Vector3 cameraUp = Vector3CrossProduct(cameraForward, cameraRight);
    cameraUp = Vector3Normalize(cameraUp);

    // Set shader uniforms (camera right/up for billboarding)
    int rightLoc = GetShaderLocation(metaballShader, "cameraRight");
    int upLoc = GetShaderLocation(metaballShader, "cameraUp");
    SetShaderValue(metaballShader, rightLoc, &cameraRight, SHADER_UNIFORM_VEC3);
    SetShaderValue(metaballShader, upLoc, &cameraUp, SHADER_UNIFORM_VEC3);

    // Begin shader mode
    BeginShaderMode(metaballShader);

    // Render each particle as a soft billboard
    for (size_t i = 0; i < microbe.softBody.particleBodyIDs.size(); i++) {
        JPH::BodyID bodyID = microbe.softBody.particleBodyIDs[i];
        JPH::RVec3 pos = bodyInterface.GetPosition(bodyID);
        Vector3 position = {(float)pos.GetX(), (float)pos.GetY(), (float)pos.GetZ()};

        // Determine particle radius and color
        float radius;
        Color color = microbe.stats.color;

        if ((int)i < microbe.softBody.skeletonParticleCount) {
            // Skeleton particle (internal)
            radius = microbe.stats.baseRadius / 15.0f;
            color.r = (unsigned char)(color.r * 0.6f);
            color.g = (unsigned char)(color.g * 0.6f);
            color.b = (unsigned char)(color.b * 0.6f);
        } else {
            // Membrane particle (surface)
            radius = microbe.stats.baseRadius / 8.0f;
        }

        // Set per-particle uniforms
        int radiusLoc = GetShaderLocation(metaballShader, "instanceRadius");
        int colorLoc = GetShaderLocation(metaballShader, "instanceColor");
        Vector4 colorVec = {color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f};

        SetShaderValue(metaballShader, radiusLoc, &radius, SHADER_UNIFORM_FLOAT);
        SetShaderValue(metaballShader, colorLoc, &colorVec, SHADER_UNIFORM_VEC4);

        // Draw billboard at particle position
        DrawMesh(billboardQuad, LoadMaterialDefault(), MatrixTranslate(position.x, position.y, position.z));
    }

    EndShaderMode();
}

} // namespace micro_idle
