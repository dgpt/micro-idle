#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "src/systems/SDFRenderSystem.h"
#include "src/systems/UpdateSDFUniforms.h"
#include "src/components/Microbe.h"
#include "src/components/Transform.h"
#include "src/components/Rendering.h"
#include "src/rendering/SDFShader.h"
#include "src/rendering/RaymarchBounds.h"
#include "src/systems/SoftBodyFactory.h"
#include "src/systems/PhysicsSystem.h"

using namespace micro_idle;
using Catch::Approx;

// Test fixture for rendering systems
class RenderingTestFixture {
public:
    RenderingTestFixture() {
        // Initialize FLECS world
        world = new flecs::world();

        // Register components
        world->component<components::Transform>();
        world->component<components::Microbe>();
        world->component<components::SDFRenderComponent>();
        world->component<components::CameraState>();

        // Initialize physics
        physics = new PhysicsSystemState();

        // Create test microbe entity
        createTestMicrobe();
    }

    ~RenderingTestFixture() {
        delete world;
        delete physics;
    }

    void createTestMicrobe() {
        // Create a test microbe with soft body
        std::vector<JPH::BodyID> skeletonBodyIDs;
        JPH::BodyID bodyID = SoftBodyFactory::CreateAmoeba(physics,
            Vector3{0.0f, 1.0f, 0.0f}, 1.0f, 1, skeletonBodyIDs);

        flecs::entity entity = world->entity();
        entity.set<components::Transform>(components::Transform{
            .position = {0.0f, 1.0f, 0.0f},
            .rotation = {0.0f, 0.0f, 0.0f, 1.0f},
            .scale = {1.0f, 1.0f, 1.0f}
        });

        entity.set<components::Microbe>(components::Microbe{
            .type = components::MicrobeType::Amoeba,
            .stats = {
                .seed = 123.0f,
                .baseRadius = 1.0f,
                .color = RED,
                .health = 100.0f,
                .energy = 100.0f
            },
            .softBody = {
                .bodyID = bodyID,
                .vertexCount = SoftBodyFactory::GetVertexCount(physics, bodyID),
                .subdivisions = 1
            }
        });

        // Create SDF render component
        components::SDFRenderComponent sdf;
        sdf.vertexCount = 0; // Will be set by UpdateSDFUniforms
        entity.set<components::SDFRenderComponent>(sdf);

        testEntity = entity;
    }

    flecs::world* world;
    PhysicsSystemState* physics;
    flecs::entity testEntity;
};

TEST_CASE_METHOD(RenderingTestFixture, "UpdateSDFUniforms - Extracts vertex positions", "[rendering]") {
    // Register the UpdateSDFUniforms system
    UpdateSDFUniforms::registerSystem(*world, physics);

    // Run the system
    world->progress();

    // Check that vertex positions were extracted
    auto sdf = testEntity.get<components::SDFRenderComponent>();
    REQUIRE(sdf != nullptr);
    REQUIRE(sdf->vertexCount > 0);
    REQUIRE(sdf->vertexCount <= 64); // Limited by our array size

    // Check that positions are reasonable (not all zero)
    bool hasNonZeroPosition = false;
    for (int i = 0; i < sdf->vertexCount; i++) {
        float dist = sqrtf(sdf->vertexPositions[i].x * sdf->vertexPositions[i].x +
                          sdf->vertexPositions[i].y * sdf->vertexPositions[i].y +
                          sdf->vertexPositions[i].z * sdf->vertexPositions[i].z);
        if (dist > 0.1f) {
            hasNonZeroPosition = true;
            break;
        }
    }
    REQUIRE(hasNonZeroPosition);
}

TEST_CASE_METHOD(RenderingTestFixture, "UpdateSDFUniforms - Handles invalid soft body", "[rendering]") {
    // Create entity with invalid soft body
    flecs::entity invalidEntity = world->entity();
    invalidEntity.set<components::Microbe>(components::Microbe{
        .softBody = {
            .bodyID = JPH::BodyID(), // Invalid body ID
            .vertexCount = 0
        }
    });

    components::SDFRenderComponent sdf;
    invalidEntity.set<components::SDFRenderComponent>(sdf);

    // Register and run system
    UpdateSDFUniforms::registerSystem(*world, physics);
    world->progress();

    // Should not crash and vertexCount should remain 0
    auto resultSdf = invalidEntity.get<components::SDFRenderComponent>();
    REQUIRE(resultSdf->vertexCount == 0);
}

TEST_CASE("SDFRenderSystem - Registers without errors", "[rendering]") {
    flecs::world world;

    // Register components
    world.component<components::Transform>();
    world.component<components::Microbe>();
    world.component<components::SDFRenderComponent>();
    world.component<components::CameraState>();

    // Should not crash
    SDFRenderSystem::registerSystem(world);

    // System should be registered
    auto system = world.lookup("SDFRenderSystem");
    REQUIRE(system != 0);
}

TEST_CASE("Rendering utilities - calculateBoundRadius", "[rendering]") {
    float baseRadius = 1.0f;
    float expected = baseRadius * 2.5f; // Default multiplier
    float result = micro_idle::rendering::calculateBoundRadius(baseRadius);
    REQUIRE(result == Approx(expected));
}

TEST_CASE("Rendering utilities - calculateBoundRadiusWithDeformation", "[rendering]") {
    float baseRadius = 1.0f;
    int vertexCount = 42;
    float maxDeformation = 1.5f;

    float result = micro_idle::rendering::calculateBoundRadiusWithDeformation(baseRadius, vertexCount, maxDeformation);
    REQUIRE(result > baseRadius);
    REQUIRE(result < baseRadius * 3.0f); // Reasonable bounds
}
