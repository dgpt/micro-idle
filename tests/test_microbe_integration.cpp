#include <stdio.h>
#include <assert.h>
#include <cmath>
#include "src/World.h"
#include "src/components/Microbe.h"
#include "src/components/Transform.h"
#include "src/systems/SoftBodyFactory.h"
#include "src/systems/PhysicsSystem.h"
#include "raylib.h"
#include <Jolt/Physics/SoftBody/SoftBodyMotionProperties.h>
#include <Jolt/Physics/Body/BodyLockInterface.h>
#include <Jolt/Physics/Body/Body.h>

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

// Test: Create amoeba entity with complete integration
static int test_create_amoeba_entity() {
    TEST("Create amoeba entity");

    World world;

    Vector3 position = {0.0f, 5.0f, 0.0f};
    float radius = 1.5f;
    Color color = RED;

    auto entity = world.createAmoeba(position, radius, color);

    // Verify entity was created
    if (!entity.is_alive()) {
        FAIL("Entity was not created");
    }

    // Verify microbe component exists
    const components::Microbe* microbe = entity.get<components::Microbe>();
    if (microbe == nullptr) {
        FAIL("Microbe component missing");
    }

    // Verify soft body was created
    if (microbe->softBody.bodyID.IsInvalid()) {
        FAIL("Soft body not created");
    }

    // Verify vertex count
    if (microbe->softBody.vertexCount != 42) {
        FAIL("Incorrect vertex count");
    }

    // Verify transform component exists
    const components::Transform* transform = entity.get<components::Transform>();
    if (transform == nullptr) {
        FAIL("Transform component missing");
    }

    // Verify initial position
    if (fabsf(transform->position.x - position.x) > 0.1f ||
        fabsf(transform->position.y - position.y) > 0.1f ||
        fabsf(transform->position.z - position.z) > 0.1f) {
        FAIL("Initial position incorrect");
    }

    PASS();
    return 0;
}

// Test: Microbe simulation updates
static int test_microbe_simulation() {
    TEST("Microbe simulation updates");

    World world;

    Vector3 position = {0.0f, 10.0f, 0.0f}; // Start high
    auto entity = world.createAmoeba(position, 1.5f, BLUE);

    const components::Microbe* microbe = entity.get<components::Microbe>();
    const components::Transform* transform = entity.get<components::Transform>();

    float initialPhase = microbe->locomotion.phase;

    // Run simulation for 1 second
    for (int i = 0; i < 60; i++) {
        world.update(1.0f / 60.0f);
    }

    // Refresh component pointers (they might have moved)
    transform = entity.get<components::Transform>();
    microbe = entity.get<components::Microbe>();

    // Phase should have progressed
    if (fabsf(microbe->locomotion.phase - initialPhase) < 0.01f) {
        FAIL("Locomotion phase should have changed");
    }

    // Entity should still be alive and have valid soft body
    if (!entity.is_alive() || microbe->softBody.bodyID.IsInvalid()) {
        FAIL("Entity should remain valid during simulation");
    }

    PASS();
    return 0;
}

// Test: Multiple amoebas interact
static int test_multiple_amoebas() {
    TEST("Multiple amoebas interact");

    World world;

    // Create 3 amoebas
    auto amoeba1 = world.createAmoeba({0.0f, 5.0f, 0.0f}, 1.5f, RED);
    auto amoeba2 = world.createAmoeba({3.0f, 5.0f, 0.0f}, 1.2f, GREEN);
    auto amoeba3 = world.createAmoeba({-3.0f, 5.0f, 0.0f}, 1.3f, BLUE);

    // Verify all created
    if (!amoeba1.is_alive() || !amoeba2.is_alive() || !amoeba3.is_alive()) {
        FAIL("Not all amoebas created");
    }

    // Run simulation
    for (int i = 0; i < 120; i++) {
        world.update(1.0f / 60.0f);
    }

    // All should still be alive
    if (!amoeba1.is_alive() || !amoeba2.is_alive() || !amoeba3.is_alive()) {
        FAIL("Amoebas should still be alive");
    }

    // All should have soft bodies
    const components::Microbe* m1 = amoeba1.get<components::Microbe>();
    const components::Microbe* m2 = amoeba2.get<components::Microbe>();
    const components::Microbe* m3 = amoeba3.get<components::Microbe>();

    if (m1->softBody.bodyID.IsInvalid() ||
        m2->softBody.bodyID.IsInvalid() ||
        m3->softBody.bodyID.IsInvalid()) {
        FAIL("All amoebas should have valid soft bodies");
    }

    PASS();
    return 0;
}

// Test: Transform sync from physics
static int test_transform_sync() {
    TEST("Transform sync from physics");

    World world;

    Vector3 initialPos = {0.0f, 10.0f, 0.0f};
    auto entity = world.createAmoeba(initialPos, 1.5f, YELLOW);

    // Step physics without updating world
    world.physics->update(1.0f / 60.0f);

    // Now update world (should sync transforms)
    world.update(1.0f / 60.0f);

    // Get soft body center of mass
    const components::Microbe* microbe = entity.get<components::Microbe>();
    JPH::BodyInterface& bodyInterface = world.physics->physicsSystem->GetBodyInterface();
    JPH::RVec3 physicsPos = bodyInterface.GetCenterOfMassPosition(microbe->softBody.bodyID);

    // Get transform position
    const components::Transform* transform = entity.get<components::Transform>();

    // Should be in sync
    if (fabsf(transform->position.x - (float)physicsPos.GetX()) > 0.01f ||
        fabsf(transform->position.y - (float)physicsPos.GetY()) > 0.01f ||
        fabsf(transform->position.z - (float)physicsPos.GetZ()) > 0.01f) {
        FAIL("Transform not synced with physics");
    }

    PASS();
    return 0;
}

// Test: Vertex extraction during simulation
static int test_vertex_extraction() {
    TEST("Vertex extraction during simulation");

    World world;

    auto entity = world.createAmoeba({0.0f, 5.0f, 0.0f}, 1.5f, MAGENTA);
    const components::Microbe* microbe = entity.get<components::Microbe>();

    // Extract vertices
    Vector3 positions[256];
    int count = SoftBodyFactory::ExtractVertexPositions(
        world.physics,
        microbe->softBody.bodyID,
        positions,
        256
    );

    // Should get 42 vertices
    if (count != 42) {
        FAIL("Should extract 42 vertices");
    }

    // Run simulation
    for (int i = 0; i < 30; i++) {
        world.update(1.0f / 60.0f);
    }

    // Extract again
    microbe = entity.get<components::Microbe>(); // Refresh pointer
    int count2 = SoftBodyFactory::ExtractVertexPositions(
        world.physics,
        microbe->softBody.bodyID,
        positions,
        256
    );

    // Should still get 42 vertices
    if (count2 != 42) {
        FAIL("Should still extract 42 vertices after simulation");
    }

    // Positions should be different (moved due to physics)
    bool positionsChanged = false;
    Vector3 firstPositions[256];
    SoftBodyFactory::ExtractVertexPositions(
        world.physics,
        microbe->softBody.bodyID,
        firstPositions,
        256
    );

    world.update(0.5f); // Half second

    Vector3 secondPositions[256];
    SoftBodyFactory::ExtractVertexPositions(
        world.physics,
        microbe->softBody.bodyID,
        secondPositions,
        256
    );

    for (int i = 0; i < count; i++) {
        float dx = firstPositions[i].x - secondPositions[i].x;
        float dy = firstPositions[i].y - secondPositions[i].y;
        float dz = firstPositions[i].z - secondPositions[i].z;
        float distance = sqrtf(dx*dx + dy*dy + dz*dz);
        if (distance > 0.01f) {
            positionsChanged = true;
            break;
        }
    }

    if (!positionsChanged) {
        FAIL("Vertex positions should change during simulation");
    }

    PASS();
    return 0;
}

// Test: EC&M locomotion affects soft body
static int test_locomotion_affects_softbody() {
    TEST("Locomotion affects soft body");

    World world;

    auto entity = world.createAmoeba({0.0f, 5.0f, 0.0f}, 1.5f, ORANGE);
    const components::Microbe* microbe = entity.get<components::Microbe>();

    // Get initial target vertex velocity
    int targetIdx = microbe->locomotion.targetVertexIndex;
    float initialSpeed;
    {
        JPH::BodyLockRead lock(world.physics->physicsSystem->GetBodyLockInterface(),
                               microbe->softBody.bodyID);
        const JPH::Body& body = lock.GetBody();
        const JPH::SoftBodyMotionProperties* motionProps =
            static_cast<const JPH::SoftBodyMotionProperties*>(body.GetMotionProperties());
        JPH::Vec3 initialVelocity = motionProps->GetVertex(targetIdx).mVelocity;
        initialSpeed = initialVelocity.Length();
    } // Release lock before update

    // Run several updates
    for (int i = 0; i < 10; i++) {
        world.update(1.0f / 60.0f);
    }

    // Get velocity again
    microbe = entity.get<components::Microbe>(); // Refresh
    float finalSpeed;
    {
        JPH::BodyLockRead lock2(world.physics->physicsSystem->GetBodyLockInterface(),
                                microbe->softBody.bodyID);
        const JPH::Body& body2 = lock2.GetBody();
        const JPH::SoftBodyMotionProperties* motionProps2 =
            static_cast<const JPH::SoftBodyMotionProperties*>(body2.GetMotionProperties());
        JPH::Vec3 finalVelocity = motionProps2->GetVertex(targetIdx).mVelocity;
        finalSpeed = finalVelocity.Length();
    } // Release lock

    // Velocity should have changed (locomotion applying forces)
    if (fabsf(finalSpeed - initialSpeed) < 0.001f) {
        FAIL("Locomotion should affect vertex velocities");
    }

    PASS();
    return 0;
}

// Test: Stress test - many amoebas
static int test_stress_many_amoebas() {
    TEST("Stress test - 10 amoebas");

    World world;

    const int NUM_AMOEBAS = 10;
    flecs::entity amoebas[NUM_AMOEBAS];

    // Create 10 amoebas in a circle
    for (int i = 0; i < NUM_AMOEBAS; i++) {
        float angle = (float)i / NUM_AMOEBAS * 2.0f * 3.14159f;
        float x = 5.0f * cosf(angle);
        float z = 5.0f * sinf(angle);
        amoebas[i] = world.createAmoeba({x, 5.0f, z}, 1.0f, RED);
    }

    // Run simulation for 2 seconds
    for (int i = 0; i < 120; i++) {
        world.update(1.0f / 60.0f);
    }

    // All should still be alive and have valid soft bodies
    for (int i = 0; i < NUM_AMOEBAS; i++) {
        if (!amoebas[i].is_alive()) {
            FAIL("Amoeba destroyed during simulation");
        }

        const components::Microbe* microbe = amoebas[i].get<components::Microbe>();
        if (microbe->softBody.bodyID.IsInvalid()) {
            FAIL("Soft body became invalid");
        }
    }

    PASS();
    return 0;
}

int test_microbe_integration_run(void) {
    printf("microbe_integration:\n");

    int fails = 0;
    fails += test_create_amoeba_entity();
    fails += test_microbe_simulation();
    fails += test_multiple_amoebas();
    fails += test_transform_sync();
    fails += test_vertex_extraction();
    fails += test_locomotion_affects_softbody();
    fails += test_stress_many_amoebas();

    if (fails == 0) {
        printf("  All tests passed (%d/%d)\n", pass_count, test_count);
    } else {
        printf("  %d/%d tests failed\n", fails, test_count);
    }

    return fails;
}
