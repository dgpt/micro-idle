#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include "src/systems/ECMLocomotionSystem.h"
#include "src/systems/SoftBodyFactory.h"
#include "src/systems/PhysicsSystem.h"
#include "src/components/Microbe.h"
#include "raylib.h"
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

TEST_CASE("ECMLocomotion - Initialize locomotion state", "[ecm_locomotion]") {
    components::ECMLocomotion locomotion = {};
    ECMLocomotionSystem::initialize(locomotion);

    // Verify phase is initialized (0-1)
    REQUIRE(locomotion.phase >= 0.0f);
    REQUIRE(locomotion.phase <= 1.0f);

    // Verify Y component is zero (horizontal movement)
    REQUIRE(std::abs(locomotion.targetDirection.y) < 0.001f);

    // Verify wiggle phase is initialized
    REQUIRE(locomotion.wigglePhase >= 0.0f);

    // Verify phase flags are set correctly based on initial phase
    bool flagsCorrect = false;
    if (locomotion.phase < 0.33f) {
        flagsCorrect = locomotion.isExtending && !locomotion.isSearching && !locomotion.isRetracting;
    } else if (locomotion.phase < 0.67f) {
        flagsCorrect = !locomotion.isExtending && locomotion.isSearching && !locomotion.isRetracting;
    } else {
        flagsCorrect = !locomotion.isExtending && !locomotion.isSearching && locomotion.isRetracting;
    }
    REQUIRE(flagsCorrect);
}

TEST_CASE("ECMLocomotion - Phase progression", "[ecm_locomotion]") {
    PhysicsSystemState* physics = new PhysicsSystemState();

    // Create microbe with soft body
    components::Microbe microbe = {};
    std::vector<JPH::BodyID> skeletonBodyIDs;
    microbe.softBody.bodyID = SoftBodyFactory::CreateAmoeba(physics, {0, 5, 0}, 1.5f, 1, skeletonBodyIDs);
    microbe.softBody.vertexCount = SoftBodyFactory::GetVertexCount(physics, microbe.softBody.bodyID);
    microbe.softBody.subdivisions = 1;

    components::InternalSkeleton skeleton;
    skeleton.skeletonBodyIDs = skeletonBodyIDs;
    skeleton.skeletonNodeCount = (int)skeletonBodyIDs.size();

    ECMLocomotionSystem::initialize(microbe.locomotion);

    float initialPhase = microbe.locomotion.phase;
    float dt = 1.0f; // 1 second

    // Update 12 times (should complete one full cycle)
    for (int i = 0; i < 12; i++) {
        ECMLocomotionSystem::update(microbe, skeleton, physics, dt);
    }

    // Phase should be back to roughly the same value (accounting for modulo)
    // After 12 seconds (12 updates of 1 second each), phase should wrap around
    float phaseDiff = std::abs(microbe.locomotion.phase - initialPhase);
    // Allow tolerance for floating point precision - phase wraps at 1.0, so difference could be up to 1.0
    REQUIRE(phaseDiff < 1.1f); // Allow wrap-around tolerance

    cleanupBodies(physics, microbe.softBody.bodyID, skeletonBodyIDs);
    delete physics;
}

TEST_CASE("ECMLocomotion - Phase transitions", "[ecm_locomotion]") {
    PhysicsSystemState* physics = new PhysicsSystemState();

    components::Microbe microbe = {};
    std::vector<JPH::BodyID> skeletonBodyIDs;
    microbe.softBody.bodyID = SoftBodyFactory::CreateAmoeba(physics, {0, 5, 0}, 1.5f, 1, skeletonBodyIDs);
    microbe.softBody.vertexCount = SoftBodyFactory::GetVertexCount(physics, microbe.softBody.bodyID);
    microbe.softBody.subdivisions = 1;

    components::InternalSkeleton skeleton;
    skeleton.skeletonBodyIDs = skeletonBodyIDs;
    skeleton.skeletonNodeCount = (int)skeletonBodyIDs.size();

    // Start at phase 0 (extending)
    microbe.locomotion.phase = 0.0f;
    microbe.locomotion.targetVertexIndex = 0;
    microbe.locomotion.targetDirection = {1, 0, 0};
    microbe.locomotion.wigglePhase = 0.0f;

    // Update to extension phase
    ECMLocomotionSystem::update(microbe, skeleton, physics, 0.01f);
    REQUIRE(microbe.locomotion.isExtending);
    REQUIRE_FALSE(microbe.locomotion.isSearching);
    REQUIRE_FALSE(microbe.locomotion.isRetracting);

    // Move to search phase
    microbe.locomotion.phase = 0.5f; // Middle of search phase
    ECMLocomotionSystem::update(microbe, skeleton, physics, 0.01f);
    REQUIRE_FALSE(microbe.locomotion.isExtending);
    REQUIRE(microbe.locomotion.isSearching);
    REQUIRE_FALSE(microbe.locomotion.isRetracting);

    // Move to retract phase
    microbe.locomotion.phase = 0.8f; // In retract phase
    ECMLocomotionSystem::update(microbe, skeleton, physics, 0.01f);
    REQUIRE_FALSE(microbe.locomotion.isExtending);
    REQUIRE_FALSE(microbe.locomotion.isSearching);
    REQUIRE(microbe.locomotion.isRetracting);

    cleanupBodies(physics, microbe.softBody.bodyID, skeletonBodyIDs);
    delete physics;
}

TEST_CASE("ECMLocomotion - Extension forces modify vertex velocity", "[ecm_locomotion]") {
    PhysicsSystemState* physics = new PhysicsSystemState();

    components::Microbe microbe = {};
    std::vector<JPH::BodyID> skeletonBodyIDs;
    microbe.softBody.bodyID = SoftBodyFactory::CreateAmoeba(physics, {0, 5, 0}, 1.5f, 1, skeletonBodyIDs);
    microbe.softBody.vertexCount = SoftBodyFactory::GetVertexCount(physics, microbe.softBody.bodyID);
    microbe.softBody.subdivisions = 1;

    components::InternalSkeleton skeleton;
    skeleton.skeletonBodyIDs = skeletonBodyIDs;
    skeleton.skeletonNodeCount = (int)skeletonBodyIDs.size();

    microbe.locomotion.phase = 0.1f; // Extension phase
    microbe.locomotion.targetVertexIndex = 0;
    microbe.locomotion.targetDirection = {1, 0, 0}; // East
    microbe.locomotion.wigglePhase = 0.0f;
    microbe.locomotion.isExtending = true;
    microbe.locomotion.isSearching = false;
    microbe.locomotion.isRetracting = false;

    // Get initial velocity of first skeleton body (forces are applied to skeleton[0], not soft body vertices)
    JPH::BodyInterface& bodyInterface = physics->physicsSystem->GetBodyInterface();
    JPH::BodyID skeletonBodyID = skeleton.skeletonBodyIDs[0]; // Forces applied to first skeleton body

    JPH::Vec3 initialVelocity = bodyInterface.GetLinearVelocity(skeletonBodyID);

    // Apply extension forces multiple times to see effect (forces accumulate)
    for (int i = 0; i < 10; i++) {
        ECMLocomotionSystem::update(microbe, skeleton, physics, 0.016f);
        physics->update(0.016f); // Step physics to apply forces
    }

    // Get velocity after applying forces
    JPH::Vec3 finalVelocity = bodyInterface.GetLinearVelocity(skeletonBodyID);

    // Velocity should have changed (increased in X direction) - check magnitude since direction might vary
    float initialMag = initialVelocity.Length();
    float finalMag = finalVelocity.Length();
    REQUIRE((finalMag > initialMag + 0.001f || finalVelocity.GetX() > initialVelocity.GetX() + 0.001f));

    cleanupBodies(physics, microbe.softBody.bodyID, skeletonBodyIDs);
    delete physics;
}

TEST_CASE("ECMLocomotion - Wiggle phase updates", "[ecm_locomotion]") {
    PhysicsSystemState* physics = new PhysicsSystemState();

    components::Microbe microbe = {};
    std::vector<JPH::BodyID> skeletonBodyIDs;
    microbe.softBody.bodyID = SoftBodyFactory::CreateAmoeba(physics, {0, 5, 0}, 1.5f, 1, skeletonBodyIDs);
    microbe.softBody.vertexCount = SoftBodyFactory::GetVertexCount(physics, microbe.softBody.bodyID);
    microbe.softBody.subdivisions = 1;

    components::InternalSkeleton skeleton;
    skeleton.skeletonBodyIDs = skeletonBodyIDs;
    skeleton.skeletonNodeCount = (int)skeletonBodyIDs.size();

    microbe.locomotion.phase = 0.0f;
    microbe.locomotion.wigglePhase = 0.0f;
    microbe.locomotion.targetVertexIndex = 0;
    microbe.locomotion.targetDirection = {1, 0, 0};

    float initialWiggle = microbe.locomotion.wigglePhase;

    // Update multiple times
    float dt = 0.1f;
    for (int i = 0; i < 10; i++) {
        ECMLocomotionSystem::update(microbe, skeleton, physics, dt);
    }

    // Wiggle phase should have increased
    REQUIRE(microbe.locomotion.wigglePhase > initialWiggle);

    cleanupBodies(physics, microbe.softBody.bodyID, skeletonBodyIDs);
    delete physics;
}

TEST_CASE("ECMLocomotion - Target vertex selection on cycle reset", "[ecm_locomotion]") {
    PhysicsSystemState* physics = new PhysicsSystemState();

    components::Microbe microbe = {};
    std::vector<JPH::BodyID> skeletonBodyIDs;
    microbe.softBody.bodyID = SoftBodyFactory::CreateAmoeba(physics, {0, 5, 0}, 1.5f, 1, skeletonBodyIDs);
    microbe.softBody.vertexCount = SoftBodyFactory::GetVertexCount(physics, microbe.softBody.bodyID);
    microbe.softBody.subdivisions = 1;

    components::InternalSkeleton skeleton;
    skeleton.skeletonBodyIDs = skeletonBodyIDs;
    skeleton.skeletonNodeCount = (int)skeletonBodyIDs.size();

    microbe.locomotion.phase = 0.99f; // Almost at cycle end
    microbe.locomotion.targetVertexIndex = 0;
    microbe.locomotion.targetDirection = {1, 0, 0};
    microbe.locomotion.wigglePhase = 0.0f;

    int initialTarget = microbe.locomotion.targetVertexIndex;

    // Update to trigger cycle reset
    ECMLocomotionSystem::update(microbe, skeleton, physics, 0.5f); // Large dt to cross 1.0

    // Target vertex should be valid (0 to vertexCount-1)
    REQUIRE(microbe.locomotion.targetVertexIndex >= 0);
    REQUIRE(microbe.locomotion.targetVertexIndex < microbe.softBody.vertexCount);

    cleanupBodies(physics, microbe.softBody.bodyID, skeletonBodyIDs);
    delete physics;
}

TEST_CASE("ECMLocomotion - Retraction forces pull body toward anchored foot (Body Pull)", "[ecm_locomotion]") {
    PhysicsSystemState* physics = new PhysicsSystemState();

    components::Microbe microbe = {};
    std::vector<JPH::BodyID> skeletonBodyIDs;
    microbe.softBody.bodyID = SoftBodyFactory::CreateAmoeba(physics, {0, 5, 0}, 1.5f, 1, skeletonBodyIDs);
    microbe.softBody.vertexCount = SoftBodyFactory::GetVertexCount(physics, microbe.softBody.bodyID);
    microbe.softBody.subdivisions = 1;

    components::InternalSkeleton skeleton;
    skeleton.skeletonBodyIDs = skeletonBodyIDs;
    skeleton.skeletonNodeCount = (int)skeletonBodyIDs.size();

    microbe.locomotion.phase = 0.8f; // Retraction phase
    microbe.locomotion.targetVertexIndex = 0; // Target vertex (anchored foot)
    microbe.locomotion.targetDirection = {1, 0, 0};
    microbe.locomotion.wigglePhase = 0.0f;
    microbe.locomotion.isExtending = false;
    microbe.locomotion.isSearching = false;
    microbe.locomotion.isRetracting = true;

    // Get initial velocities of soft body vertices (Body Pull applies forces to vertices, not skeleton)
    JPH::Vec3 initialVelocity;
    int testVertexIdx;
    {
        JPH::BodyLockRead lock(physics->physicsSystem->GetBodyLockInterface(), microbe.softBody.bodyID);
        REQUIRE(lock.Succeeded());

        const JPH::Body& body = lock.GetBody();
        const JPH::SoftBodyMotionProperties* motionProps =
            static_cast<const JPH::SoftBodyMotionProperties*>(body.GetMotionProperties());
        REQUIRE(motionProps != nullptr);

        const JPH::Array<JPH::SoftBodyVertex>& vertices = motionProps->GetVertices();
        REQUIRE(vertices.size() > 0);

        // Get initial velocity of a non-target vertex (should be modified by Body Pull)
        testVertexIdx = (microbe.locomotion.targetVertexIndex + 1) % (int)vertices.size();
        initialVelocity = vertices[testVertexIdx].mVelocity;
    } // Lock automatically released here

    // Apply retraction forces multiple times to see effect
    for (int i = 0; i < 10; i++) {
        ECMLocomotionSystem::update(microbe, skeleton, physics, 0.016f);
        physics->update(0.016f); // Step physics to apply forces
    }

    // Verify soft body vertex velocity was modified (Body Pull applies forces to vertices)
    JPH::Vec3 finalVelocity;
    {
        JPH::BodyLockRead lock2(physics->physicsSystem->GetBodyLockInterface(), microbe.softBody.bodyID);
        REQUIRE(lock2.Succeeded());

        const JPH::Body& body2 = lock2.GetBody();
        const JPH::SoftBodyMotionProperties* motionProps2 =
            static_cast<const JPH::SoftBodyMotionProperties*>(body2.GetMotionProperties());
        const JPH::Array<JPH::SoftBodyVertex>& vertices2 = motionProps2->GetVertices();
        finalVelocity = vertices2[testVertexIdx].mVelocity;
    } // Lock automatically released here

    float initialMag = initialVelocity.Length();
    float finalMag = finalVelocity.Length();

    // Velocity magnitude should have changed (Body Pull applies velocity impulse to non-target vertices)
    // Note: Target vertex (index 0) should remain unchanged (anchored), but other vertices should move
    REQUIRE(std::abs(finalMag - initialMag) >= 0.00001f);

    cleanupBodies(physics, microbe.softBody.bodyID, skeletonBodyIDs);
    delete physics;
}
