#include <stdio.h>
#include <assert.h>
#include <cmath>
#include "src/systems/SoftBodyFactory.h"
#include "src/systems/PhysicsSystem.h"
#include "raylib.h"
#include <Jolt/Physics/SoftBody/SoftBodyMotionProperties.h>
#include <Jolt/Physics/Body/BodyLockInterface.h>
#include <Jolt/Physics/Body/Body.h>
#include <vector>

using namespace micro_idle;

static int test_count = 0;
static int pass_count = 0;

#define TEST(name) \
    printf("  Testing: %s...", name); \
    test_count++;

#define PASS() \
    printf(" PASS\n"); \
    pass_count++;

#define FAIL(msg) \
    printf(" FAIL: %s\n", msg); \
    return 1;

// Test: Create amoeba soft body
static int test_create_amoeba() {
    TEST("Create amoeba soft body");

    PhysicsSystemState* physics = new PhysicsSystemState();

    Vector3 position = {0.0f, 5.0f, 0.0f};
    float radius = 2.0f;
    int subdivisions = 1; // 42 vertices

    std::vector<JPH::BodyID> skeletonBodyIDs;
    JPH::BodyID bodyID = SoftBodyFactory::CreateAmoeba(physics, position, radius, subdivisions, skeletonBodyIDs);

    if (bodyID.IsInvalid()) {
        delete physics;
        FAIL("Failed to create soft body");
    }

    // Verify body was created
    JPH::BodyInterface& bodyInterface = physics->physicsSystem->GetBodyInterface();
    if (!bodyInterface.IsAdded(bodyID)) {
        // Clean up skeleton bodies
        for (JPH::BodyID skeletonID : skeletonBodyIDs) {
            if (!skeletonID.IsInvalid()) {
                bodyInterface.RemoveBody(skeletonID);
                bodyInterface.DestroyBody(skeletonID);
            }
        }
        delete physics;
        FAIL("Body was not added to physics system");
    }

    // Clean up skeleton bodies
    for (JPH::BodyID skeletonID : skeletonBodyIDs) {
        if (!skeletonID.IsInvalid()) {
            bodyInterface.RemoveBody(skeletonID);
            bodyInterface.DestroyBody(skeletonID);
        }
    }

    // Clean up soft body
    bodyInterface.RemoveBody(bodyID);
    bodyInterface.DestroyBody(bodyID);
    delete physics;

    PASS();
    return 0;
}

// Test: Verify vertex count
static int test_vertex_count() {
    TEST("Verify vertex count");

    PhysicsSystemState* physics = new PhysicsSystemState();

    Vector3 position = {0.0f, 5.0f, 0.0f};
    float radius = 1.5f;

    // Test subdivision 0 (12 vertices)
    std::vector<JPH::BodyID> skeleton0;
    JPH::BodyID bodyID0 = SoftBodyFactory::CreateAmoeba(physics, position, radius, 0, skeleton0);
    int count0 = SoftBodyFactory::GetVertexCount(physics, bodyID0);
    if (count0 != 12) {
        delete physics;
        FAIL("Subdivision 0 should have 12 vertices");
    }

    // Test subdivision 1 (42 vertices)
    std::vector<JPH::BodyID> skeleton1;
    JPH::BodyID bodyID1 = SoftBodyFactory::CreateAmoeba(physics, {1.0f, 5.0f, 0.0f}, radius, 1, skeleton1);
    int count1 = SoftBodyFactory::GetVertexCount(physics, bodyID1);
    if (count1 != 42) {
        delete physics;
        FAIL("Subdivision 1 should have 42 vertices");
    }

    // Test subdivision 2 (162 vertices)
    std::vector<JPH::BodyID> skeleton2;
    JPH::BodyID bodyID2 = SoftBodyFactory::CreateAmoeba(physics, {2.0f, 5.0f, 0.0f}, radius, 2, skeleton2);
    int count2 = SoftBodyFactory::GetVertexCount(physics, bodyID2);
    if (count2 != 162) {
        delete physics;
        FAIL("Subdivision 2 should have 162 vertices");
    }

    // Clean up skeleton bodies
    JPH::BodyInterface& bodyInterface = physics->physicsSystem->GetBodyInterface();
    for (JPH::BodyID skeletonID : skeleton0) {
        if (!skeletonID.IsInvalid()) {
            bodyInterface.RemoveBody(skeletonID);
            bodyInterface.DestroyBody(skeletonID);
        }
    }
    for (JPH::BodyID skeletonID : skeleton1) {
        if (!skeletonID.IsInvalid()) {
            bodyInterface.RemoveBody(skeletonID);
            bodyInterface.DestroyBody(skeletonID);
        }
    }
    for (JPH::BodyID skeletonID : skeleton2) {
        if (!skeletonID.IsInvalid()) {
            bodyInterface.RemoveBody(skeletonID);
            bodyInterface.DestroyBody(skeletonID);
        }
    }

    // Clean up soft bodies
    bodyInterface.RemoveBody(bodyID0);
    bodyInterface.DestroyBody(bodyID0);
    bodyInterface.RemoveBody(bodyID1);
    bodyInterface.DestroyBody(bodyID1);
    bodyInterface.RemoveBody(bodyID2);
    bodyInterface.DestroyBody(bodyID2);
    delete physics;

    PASS();
    return 0;
}

// Test: Extract vertex positions
static int test_extract_positions() {
    TEST("Extract vertex positions");

    PhysicsSystemState* physics = new PhysicsSystemState();

    Vector3 position = {0.0f, 5.0f, 0.0f};
    float radius = 2.0f;
    int subdivisions = 1; // 42 vertices

    std::vector<JPH::BodyID> skeletonBodyIDs;
    JPH::BodyID bodyID = SoftBodyFactory::CreateAmoeba(physics, position, radius, subdivisions, skeletonBodyIDs);

    // Extract positions
    Vector3 positions[256];
    int count = SoftBodyFactory::ExtractVertexPositions(physics, bodyID, positions, 256);

    if (count != 42) {
        delete physics;
        FAIL("Should extract 42 positions");
    }

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
    if (minDistance < radius * 0.5f || maxDistance > radius * 1.5f) {
        printf("\n    Distance range: %.2f to %.2f (radius=%.2f)\n", minDistance, maxDistance, radius);
        delete physics;
        FAIL("Vertices should be roughly at sphere radius");
    }

    // Clean up skeleton bodies
    JPH::BodyInterface& bodyInterface = physics->physicsSystem->GetBodyInterface();
    for (JPH::BodyID skeletonID : skeletonBodyIDs) {
        if (!skeletonID.IsInvalid()) {
            bodyInterface.RemoveBody(skeletonID);
            bodyInterface.DestroyBody(skeletonID);
        }
    }

    // Clean up soft body
    bodyInterface.RemoveBody(bodyID);
    bodyInterface.DestroyBody(bodyID);
    delete physics;

    PASS();
    return 0;
}

// Test: Soft body properties
static int test_softbody_properties() {
    TEST("Soft body properties");

    PhysicsSystemState* physics = new PhysicsSystemState();

    Vector3 position = {0.0f, 5.0f, 0.0f};
    float radius = 2.0f;

    std::vector<JPH::BodyID> skeletonBodyIDs;
    JPH::BodyID bodyID = SoftBodyFactory::CreateAmoeba(physics, position, radius, 1, skeletonBodyIDs);

    // Lock and verify properties (use scope to ensure lock is released)
    {
        JPH::BodyLockRead lock(physics->physicsSystem->GetBodyLockInterface(), bodyID);
        if (!lock.Succeeded()) {
            delete physics;
            FAIL("Failed to lock body");
        }

        const JPH::Body& body = lock.GetBody();
        const JPH::SoftBodyMotionProperties* motionProps =
            static_cast<const JPH::SoftBodyMotionProperties*>(body.GetMotionProperties());

        if (motionProps == nullptr) {
            delete physics;
            FAIL("Motion properties are null");
        }

        // Verify pressure is set (we use 5.0f for friction-based grip-and-stretch model)
        float pressure = motionProps->GetPressure();
        if (pressure < 1.0f || pressure > 20.0f) {
            delete physics;
            FAIL("Pressure out of expected range");
        }

        // Verify iteration count
        uint32_t iterations = motionProps->GetNumIterations();
        if (iterations < 5 || iterations > 20) {
            delete physics;
            FAIL("Iteration count out of expected range");
        }
    } // Lock released here

    // Clean up skeleton bodies
    JPH::BodyInterface& bodyInterface = physics->physicsSystem->GetBodyInterface();
    for (JPH::BodyID skeletonID : skeletonBodyIDs) {
        if (!skeletonID.IsInvalid()) {
            bodyInterface.RemoveBody(skeletonID);
            bodyInterface.DestroyBody(skeletonID);
        }
    }

    // Clean up soft body
    bodyInterface.RemoveBody(bodyID);
    bodyInterface.DestroyBody(bodyID);
    delete physics;

    PASS();
    return 0;
}

// Test: Invalid body ID handling
static int test_invalid_body() {
    TEST("Invalid body ID handling");

    PhysicsSystemState* physics = new PhysicsSystemState();

    JPH::BodyID invalidID;

    // Should return 0 for invalid body
    int count = SoftBodyFactory::GetVertexCount(physics, invalidID);
    if (count != 0) {
        delete physics;
        FAIL("Should return 0 for invalid body");
    }

    // Should return 0 for extraction
    Vector3 positions[256];
    count = SoftBodyFactory::ExtractVertexPositions(physics, invalidID, positions, 256);
    if (count != 0) {
        delete physics;
        FAIL("Should return 0 for invalid body extraction");
    }

    delete physics;

    PASS();
    return 0;
}

// Test: Simulation step (verify soft body responds to physics)
static int test_simulation_step() {
    TEST("Simulation step");

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
    if (finalPos.GetY() >= initialPos.GetY() - 0.01f) {
        // Clean up skeleton bodies
        for (JPH::BodyID skeletonID : skeletonBodyIDs) {
            if (!skeletonID.IsInvalid()) {
                bodyInterface.RemoveBody(skeletonID);
                bodyInterface.DestroyBody(skeletonID);
            }
        }
        bodyInterface.RemoveBody(bodyID);
        bodyInterface.DestroyBody(bodyID);
        delete physics;
        FAIL("Soft body should have fallen due to gravity");
    }

    // Clean up skeleton bodies
    for (JPH::BodyID skeletonID : skeletonBodyIDs) {
        if (!skeletonID.IsInvalid()) {
            bodyInterface.RemoveBody(skeletonID);
            bodyInterface.DestroyBody(skeletonID);
        }
    }

    // Clean up soft body
    bodyInterface.RemoveBody(bodyID);
    bodyInterface.DestroyBody(bodyID);
    delete physics;

    PASS();
    return 0;
}

int test_softbody_factory_run(void) {
    printf("softbody_factory:\n");

    int fails = 0;
    fails += test_create_amoeba();
    fails += test_vertex_count();
    fails += test_extract_positions();
    fails += test_softbody_properties();
    fails += test_invalid_body();
    fails += test_simulation_step();

    if (fails == 0) {
        printf("  All tests passed (%d/%d)\n", pass_count, test_count);
    } else {
        printf("  %d/%d tests failed\n", fails, test_count);
    }

    return fails;
}
