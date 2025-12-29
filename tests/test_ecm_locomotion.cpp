#include <stdio.h>
#include <assert.h>
#include <cmath>
#include "src/systems/ECMLocomotionSystem.h"
#include "src/systems/SoftBodyFactory.h"
#include "src/systems/PhysicsSystem.h"
#include "src/components/Microbe.h"
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

// Test: Initialize locomotion state
static int test_initialize_locomotion() {
    TEST("Initialize locomotion state");

    components::ECMLocomotion locomotion = {};
    ECMLocomotionSystem::initialize(locomotion);

    // Verify phase is initialized (0-1)
    if (locomotion.phase < 0.0f || locomotion.phase > 1.0f) {
        FAIL("Phase should be 0-1");
    }

    // Verify target direction is normalized-ish (in XZ plane)
    float dirLength = sqrtf(
        locomotion.targetDirection.x * locomotion.targetDirection.x +
        locomotion.targetDirection.z * locomotion.targetDirection.z
    );
    if (dirLength < 0.9f || dirLength > 1.1f) {
        FAIL("Target direction should be normalized in XZ plane");
    }

    // Verify Y component is zero (horizontal movement)
    if (fabsf(locomotion.targetDirection.y) > 0.001f) {
        FAIL("Target direction Y should be zero");
    }

    // Verify wiggle phase is initialized
    if (locomotion.wigglePhase < 0.0f) {
        FAIL("Wiggle phase should be non-negative");
    }

    // Verify phase flags are set correctly based on initial phase
    bool flagsCorrect = false;
    if (locomotion.phase < 0.33f) {
        flagsCorrect = locomotion.isExtending && !locomotion.isSearching && !locomotion.isRetracting;
    } else if (locomotion.phase < 0.67f) {
        flagsCorrect = !locomotion.isExtending && locomotion.isSearching && !locomotion.isRetracting;
    } else {
        flagsCorrect = !locomotion.isExtending && !locomotion.isSearching && locomotion.isRetracting;
    }

    if (!flagsCorrect) {
        FAIL("Phase flags not set correctly");
    }

    PASS();
    return 0;
}

// Test: Phase progression
static int test_phase_progression() {
    TEST("Phase progression");

    PhysicsSystemState* physics = new PhysicsSystemState();

    // Create microbe with soft body
    components::Microbe microbe = {};
    microbe.softBody.bodyID = SoftBodyFactory::CreateAmoeba(physics, {0, 5, 0}, 1.5f, 1);
    microbe.softBody.vertexCount = SoftBodyFactory::GetVertexCount(physics, microbe.softBody.bodyID);
    microbe.softBody.subdivisions = 1;

    ECMLocomotionSystem::initialize(microbe.locomotion);

    float initialPhase = microbe.locomotion.phase;
    float dt = 1.0f; // 1 second

    // Update 12 times (should complete one full cycle)
    for (int i = 0; i < 12; i++) {
        ECMLocomotionSystem::update(microbe, physics, dt);
    }

    // Phase should be back to roughly the same value (accounting for modulo)
    float phaseDiff = fabsf(microbe.locomotion.phase - initialPhase);
    if (phaseDiff > 0.1f) { // Allow some tolerance
        delete physics;
        FAIL("Phase should return to initial value after 12 seconds");
    }

    // Clean up
    JPH::BodyInterface& bodyInterface = physics->physicsSystem->GetBodyInterface();
    bodyInterface.RemoveBody(microbe.softBody.bodyID);
    bodyInterface.DestroyBody(microbe.softBody.bodyID);
    delete physics;

    PASS();
    return 0;
}

// Test: Phase transitions
static int test_phase_transitions() {
    TEST("Phase transitions");

    PhysicsSystemState* physics = new PhysicsSystemState();

    components::Microbe microbe = {};
    microbe.softBody.bodyID = SoftBodyFactory::CreateAmoeba(physics, {0, 5, 0}, 1.5f, 1);
    microbe.softBody.vertexCount = SoftBodyFactory::GetVertexCount(physics, microbe.softBody.bodyID);
    microbe.softBody.subdivisions = 1;

    // Start at phase 0 (extending)
    microbe.locomotion.phase = 0.0f;
    microbe.locomotion.targetVertexIndex = 0;
    microbe.locomotion.targetDirection = {1, 0, 0};
    microbe.locomotion.wigglePhase = 0.0f;

    // Update to extension phase
    ECMLocomotionSystem::update(microbe, physics, 0.01f);
    if (!microbe.locomotion.isExtending || microbe.locomotion.isSearching || microbe.locomotion.isRetracting) {
        delete physics;
        FAIL("Should be in extending phase");
    }

    // Move to search phase
    microbe.locomotion.phase = 0.5f; // Middle of search phase
    ECMLocomotionSystem::update(microbe, physics, 0.01f);
    if (microbe.locomotion.isExtending || !microbe.locomotion.isSearching || microbe.locomotion.isRetracting) {
        delete physics;
        FAIL("Should be in searching phase");
    }

    // Move to retract phase
    microbe.locomotion.phase = 0.8f; // In retract phase
    ECMLocomotionSystem::update(microbe, physics, 0.01f);
    if (microbe.locomotion.isExtending || microbe.locomotion.isSearching || !microbe.locomotion.isRetracting) {
        delete physics;
        FAIL("Should be in retracting phase");
    }

    // Clean up
    JPH::BodyInterface& bodyInterface = physics->physicsSystem->GetBodyInterface();
    bodyInterface.RemoveBody(microbe.softBody.bodyID);
    bodyInterface.DestroyBody(microbe.softBody.bodyID);
    delete physics;

    PASS();
    return 0;
}

// Test: Extension forces modify vertex velocity
static int test_extension_forces() {
    TEST("Extension forces modify vertex velocity");

    PhysicsSystemState* physics = new PhysicsSystemState();

    components::Microbe microbe = {};
    microbe.softBody.bodyID = SoftBodyFactory::CreateAmoeba(physics, {0, 5, 0}, 1.5f, 1);
    microbe.softBody.vertexCount = SoftBodyFactory::GetVertexCount(physics, microbe.softBody.bodyID);
    microbe.softBody.subdivisions = 1;

    microbe.locomotion.phase = 0.1f; // Extension phase
    microbe.locomotion.targetVertexIndex = 0;
    microbe.locomotion.targetDirection = {1, 0, 0}; // East
    microbe.locomotion.wigglePhase = 0.0f;
    microbe.locomotion.isExtending = true;
    microbe.locomotion.isSearching = false;
    microbe.locomotion.isRetracting = false;

    // Get initial velocity of target vertex
    JPH::Vec3 initialVelocity;
    {
        JPH::BodyLockRead lock(physics->physicsSystem->GetBodyLockInterface(), microbe.softBody.bodyID);
        const JPH::Body& body = lock.GetBody();
        const JPH::SoftBodyMotionProperties* motionProps =
            static_cast<const JPH::SoftBodyMotionProperties*>(body.GetMotionProperties());
        initialVelocity = motionProps->GetVertex(0).mVelocity;
    } // Release lock before calling update

    // Apply extension forces
    ECMLocomotionSystem::update(microbe, physics, 0.016f);

    // Get velocity after applying forces
    JPH::Vec3 finalVelocity;
    {
        JPH::BodyLockRead lock2(physics->physicsSystem->GetBodyLockInterface(), microbe.softBody.bodyID);
        const JPH::Body& body2 = lock2.GetBody();
        const JPH::SoftBodyMotionProperties* motionProps2 =
            static_cast<const JPH::SoftBodyMotionProperties*>(body2.GetMotionProperties());
        finalVelocity = motionProps2->GetVertex(0).mVelocity;
    } // Release lock

    // Velocity should have changed (increased in X direction)
    if (finalVelocity.GetX() <= initialVelocity.GetX()) {
        delete physics;
        FAIL("Velocity should increase in target direction");
    }

    // Clean up
    JPH::BodyInterface& bodyInterface = physics->physicsSystem->GetBodyInterface();
    bodyInterface.RemoveBody(microbe.softBody.bodyID);
    bodyInterface.DestroyBody(microbe.softBody.bodyID);
    delete physics;

    PASS();
    return 0;
}

// Test: Wiggle phase updates
static int test_wiggle_phase() {
    TEST("Wiggle phase updates");

    PhysicsSystemState* physics = new PhysicsSystemState();

    components::Microbe microbe = {};
    microbe.softBody.bodyID = SoftBodyFactory::CreateAmoeba(physics, {0, 5, 0}, 1.5f, 1);
    microbe.softBody.vertexCount = SoftBodyFactory::GetVertexCount(physics, microbe.softBody.bodyID);
    microbe.softBody.subdivisions = 1;

    microbe.locomotion.phase = 0.0f;
    microbe.locomotion.wigglePhase = 0.0f;
    microbe.locomotion.targetVertexIndex = 0;
    microbe.locomotion.targetDirection = {1, 0, 0};

    float initialWiggle = microbe.locomotion.wigglePhase;

    // Update multiple times
    float dt = 0.1f;
    for (int i = 0; i < 10; i++) {
        ECMLocomotionSystem::update(microbe, physics, dt);
    }

    // Wiggle phase should have increased
    if (microbe.locomotion.wigglePhase <= initialWiggle) {
        delete physics;
        FAIL("Wiggle phase should increase over time");
    }

    // Clean up
    JPH::BodyInterface& bodyInterface = physics->physicsSystem->GetBodyInterface();
    bodyInterface.RemoveBody(microbe.softBody.bodyID);
    bodyInterface.DestroyBody(microbe.softBody.bodyID);
    delete physics;

    PASS();
    return 0;
}

// Test: Target vertex selection on cycle reset
static int test_target_selection() {
    TEST("Target vertex selection on cycle reset");

    PhysicsSystemState* physics = new PhysicsSystemState();

    components::Microbe microbe = {};
    microbe.softBody.bodyID = SoftBodyFactory::CreateAmoeba(physics, {0, 5, 0}, 1.5f, 1);
    microbe.softBody.vertexCount = SoftBodyFactory::GetVertexCount(physics, microbe.softBody.bodyID);
    microbe.softBody.subdivisions = 1;

    microbe.locomotion.phase = 0.99f; // Almost at cycle end
    microbe.locomotion.targetVertexIndex = 0;
    microbe.locomotion.targetDirection = {1, 0, 0};
    microbe.locomotion.wigglePhase = 0.0f;

    int initialTarget = microbe.locomotion.targetVertexIndex;

    // Update to trigger cycle reset
    ECMLocomotionSystem::update(microbe, physics, 0.5f); // Large dt to cross 1.0

    // Target vertex should be valid (0 to vertexCount-1)
    if (microbe.locomotion.targetVertexIndex < 0 ||
        microbe.locomotion.targetVertexIndex >= microbe.softBody.vertexCount) {
        delete physics;
        FAIL("Target vertex index out of bounds");
    }

    // Clean up
    JPH::BodyInterface& bodyInterface = physics->physicsSystem->GetBodyInterface();
    bodyInterface.RemoveBody(microbe.softBody.bodyID);
    bodyInterface.DestroyBody(microbe.softBody.bodyID);
    delete physics;

    PASS();
    return 0;
}

// Test: Retraction forces pull toward center
static int test_retraction_forces() {
    TEST("Retraction forces pull toward center");

    PhysicsSystemState* physics = new PhysicsSystemState();

    components::Microbe microbe = {};
    microbe.softBody.bodyID = SoftBodyFactory::CreateAmoeba(physics, {0, 5, 0}, 1.5f, 1);
    microbe.softBody.vertexCount = SoftBodyFactory::GetVertexCount(physics, microbe.softBody.bodyID);
    microbe.softBody.subdivisions = 1;

    microbe.locomotion.phase = 0.8f; // Retraction phase
    microbe.locomotion.targetVertexIndex = 0;
    microbe.locomotion.targetDirection = {1, 0, 0};
    microbe.locomotion.wigglePhase = 0.0f;
    microbe.locomotion.isExtending = false;
    microbe.locomotion.isSearching = false;
    microbe.locomotion.isRetracting = true;

    // Apply retraction forces
    ECMLocomotionSystem::update(microbe, physics, 0.016f);

    // Verify velocity was modified (this is indirect, but we can check it changed)
    float velMagnitude = 0.0f;
    {
        JPH::BodyLockRead lock(physics->physicsSystem->GetBodyLockInterface(), microbe.softBody.bodyID);
        const JPH::Body& body = lock.GetBody();
        const JPH::SoftBodyMotionProperties* motionProps =
            static_cast<const JPH::SoftBodyMotionProperties*>(body.GetMotionProperties());
        JPH::Vec3 velocity = motionProps->GetVertex(0).mVelocity;
        velMagnitude = velocity.Length();
    } // Release lock

    // Velocity magnitude should be non-zero (some force was applied)
    if (velMagnitude < 0.0001f) {
        delete physics;
        FAIL("Retraction should apply some velocity");
    }

    // Clean up
    JPH::BodyInterface& bodyInterface = physics->physicsSystem->GetBodyInterface();
    bodyInterface.RemoveBody(microbe.softBody.bodyID);
    bodyInterface.DestroyBody(microbe.softBody.bodyID);
    delete physics;

    PASS();
    return 0;
}

int test_ecm_locomotion_run(void) {
    printf("ecm_locomotion:\n");

    int fails = 0;
    fails += test_initialize_locomotion();
    fails += test_phase_progression();
    fails += test_phase_transitions();
    fails += test_extension_forces();
    fails += test_wiggle_phase();
    fails += test_target_selection();
    fails += test_retraction_forces();

    if (fails == 0) {
        printf("  All tests passed (%d/%d)\n", pass_count, test_count);
    } else {
        printf("  %d/%d tests failed\n", fails, test_count);
    }

    return fails;
}
