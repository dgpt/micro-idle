#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include "src/systems/SoftBodyFactory.h"
#include "src/systems/PhysicsSystem.h"
#include "raylib.h"
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/SoftBody/SoftBodyMotionProperties.h>
#include <Jolt/Physics/Body/BodyLockInterface.h>
#include <Jolt/Physics/Body/Body.h>
#include <vector>

using namespace micro_idle;
using Catch::Approx;

// Helper function to clean up skeleton bodies and soft body
static void cleanupBodies(PhysicsSystemState* physics, JPH::BodyID bodyID, std::vector<JPH::BodyID>& skeletonBodyIDs) {
    JPH::BodyInterface& bodyInterface = physics->physicsSystem->GetBodyInterface();

    // Clean up skeleton bodies
    for (JPH::BodyID skeletonID : skeletonBodyIDs) {
        if (!skeletonID.IsInvalid()) {
            bodyInterface.RemoveBody(skeletonID);
            bodyInterface.DestroyBody(skeletonID);
        }
    }

    // Clean up soft body
    if (!bodyID.IsInvalid()) {
        bodyInterface.RemoveBody(bodyID);
        bodyInterface.DestroyBody(bodyID);
    }
}

TEST_CASE("SoftBodyFactory - Create amoeba soft body", "[softbody_factory]") {
    PhysicsSystemState* physics = new PhysicsSystemState();

    Vector3 position = {0.0f, 5.0f, 0.0f};
    float radius = 2.0f;
    int subdivisions = 1; // 42 vertices

    std::vector<JPH::BodyID> skeletonBodyIDs;
    JPH::BodyID bodyID = SoftBodyFactory::CreateAmoeba(physics, position, radius, subdivisions, skeletonBodyIDs);

    REQUIRE_FALSE(bodyID.IsInvalid());

    // Verify body was created
    JPH::BodyInterface& bodyInterface = physics->physicsSystem->GetBodyInterface();
    REQUIRE(bodyInterface.IsAdded(bodyID));

    cleanupBodies(physics, bodyID, skeletonBodyIDs);
    delete physics;
}

TEST_CASE("SoftBodyFactory - Verify vertex count", "[softbody_factory]") {
    PhysicsSystemState* physics = new PhysicsSystemState();

    Vector3 position = {0.0f, 5.0f, 0.0f};
    float radius = 1.5f;

    // Test subdivision 0 (12 vertices)
    std::vector<JPH::BodyID> skeleton0;
    JPH::BodyID bodyID0 = SoftBodyFactory::CreateAmoeba(physics, position, radius, 0, skeleton0);
    REQUIRE(SoftBodyFactory::GetVertexCount(physics, bodyID0) == 12);

    // Test subdivision 1 (42 vertices)
    std::vector<JPH::BodyID> skeleton1;
    JPH::BodyID bodyID1 = SoftBodyFactory::CreateAmoeba(physics, {1.0f, 5.0f, 0.0f}, radius, 1, skeleton1);
    REQUIRE(SoftBodyFactory::GetVertexCount(physics, bodyID1) == 42);

    // Test subdivision 2 (162 vertices)
    std::vector<JPH::BodyID> skeleton2;
    JPH::BodyID bodyID2 = SoftBodyFactory::CreateAmoeba(physics, {2.0f, 5.0f, 0.0f}, radius, 2, skeleton2);
    REQUIRE(SoftBodyFactory::GetVertexCount(physics, bodyID2) == 162);

    cleanupBodies(physics, bodyID0, skeleton0);
    cleanupBodies(physics, bodyID1, skeleton1);
    cleanupBodies(physics, bodyID2, skeleton2);
    delete physics;
}

TEST_CASE("SoftBodyFactory - Extract vertex positions", "[softbody_factory]") {
    PhysicsSystemState* physics = new PhysicsSystemState();

    Vector3 position = {0.0f, 5.0f, 0.0f};
    float radius = 2.0f;
    int subdivisions = 1; // 42 vertices

    std::vector<JPH::BodyID> skeletonBodyIDs;
    JPH::BodyID bodyID = SoftBodyFactory::CreateAmoeba(physics, position, radius, subdivisions, skeletonBodyIDs);

    // Extract positions
    Vector3 positions[256];
    int count = SoftBodyFactory::ExtractVertexPositions(physics, bodyID, positions, 256);

    REQUIRE(count == 42);

    // Verify all positions are within reasonable bounds
    // Vertices are initialized on sphere surface at exact radius
    float minDistance = 99999.0f;
    float maxDistance = 0.0f;
    for (int i = 0; i < count; i++) {
        float dx = positions[i].x - position.x;
        float dy = positions[i].y - position.y;
        float dz = positions[i].z - position.z;
        float distance = sqrtf(dx*dx + dy*dy + dz*dz);

        if (distance > maxDistance) maxDistance = distance;
        if (distance < minDistance) minDistance = distance;
    }

    // Vertices should be roughly at radius distance (within 50% tolerance for initial state)
    REQUIRE(minDistance >= radius * 0.25f);
    REQUIRE(maxDistance <= radius * 1.5f);

    cleanupBodies(physics, bodyID, skeletonBodyIDs);
    delete physics;
}

TEST_CASE("SoftBodyFactory - Soft body properties", "[softbody_factory]") {
    PhysicsSystemState* physics = new PhysicsSystemState();

    Vector3 position = {0.0f, 5.0f, 0.0f};
    float radius = 2.0f;

    std::vector<JPH::BodyID> skeletonBodyIDs;
    JPH::BodyID bodyID = SoftBodyFactory::CreateAmoeba(physics, position, radius, 1, skeletonBodyIDs);

    // Lock and verify properties (use scope to ensure lock is released)
    {
        JPH::BodyLockRead lock(physics->physicsSystem->GetBodyLockInterface(), bodyID);
        REQUIRE(lock.Succeeded());

        const JPH::Body& body = lock.GetBody();
        const JPH::SoftBodyMotionProperties* motionProps =
            static_cast<const JPH::SoftBodyMotionProperties*>(body.GetMotionProperties());

        if (motionProps != nullptr) {
            // Verify pressure is set (we use 5.0f for friction-based grip-and-stretch model)
            float pressure = motionProps->GetPressure();
            REQUIRE(pressure >= 0.4f);
            REQUIRE(pressure <= 20.0f);

            // Verify iteration count
            uint32_t iterations = motionProps->GetNumIterations();
            REQUIRE(iterations >= 5);
            REQUIRE(iterations <= 24);
        } else {
            REQUIRE(false); // motionProps should not be null
        }
    } // Lock released here

    cleanupBodies(physics, bodyID, skeletonBodyIDs);
    delete physics;
}

TEST_CASE("SoftBodyFactory - Invalid body ID handling", "[softbody_factory]") {
    PhysicsSystemState* physics = new PhysicsSystemState();

    JPH::BodyID invalidID;

    // Should return 0 for invalid body
    REQUIRE(SoftBodyFactory::GetVertexCount(physics, invalidID) == 0);

    // Should return 0 for extraction
    Vector3 positions[256];
    REQUIRE(SoftBodyFactory::ExtractVertexPositions(physics, invalidID, positions, 256) == 0);

    delete physics;
}

TEST_CASE("SoftBodyFactory - Simulation step", "[softbody_factory]") {
    PhysicsSystemState* physics = new PhysicsSystemState();

    Vector3 position = {0.0f, 10.0f, 0.0f}; // Start high up
    float radius = 1.5f;

    std::vector<JPH::BodyID> skeletonBodyIDs;
    JPH::BodyID bodyID = SoftBodyFactory::CreateAmoeba(physics, position, radius, 1, skeletonBodyIDs);

    // Enable gravity for this test
    physics->physicsSystem->SetGravity(JPH::Vec3(0, -9.81f, 0));

    // Get initial position
    JPH::BodyInterface& bodyInterface = physics->physicsSystem->GetBodyInterface();
    JPH::RVec3 initialPos = bodyInterface.GetCenterOfMassPosition(bodyID);

    // Step physics multiple times
    for (int i = 0; i < 60; i++) { // 1 second at 60Hz
        physics->update(1.0f / 60.0f);
    }

    // Get final position
    JPH::RVec3 finalPos = bodyInterface.GetCenterOfMassPosition(bodyID);

    // Should have fallen due to gravity (or at least moved due to physics)
    REQUIRE(finalPos.GetY() < initialPos.GetY() - 0.01f);

    cleanupBodies(physics, bodyID, skeletonBodyIDs);
    delete physics;
}
