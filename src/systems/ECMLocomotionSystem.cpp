#include "ECMLocomotionSystem.h"
#include <cmath>
#include <stdio.h>
#include <cstdint>
#include <Jolt/Physics/SoftBody/SoftBodyMotionProperties.h>
#include <Jolt/Physics/Body/BodyLockInterface.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyInterface.h>

namespace micro_idle {

// Simple hash function to generate pseudo-random values from seed
static float hashToFloat(float seed, int iteration) {
    // Simple multiplicative hash
    uint32_t hash = (uint32_t)(seed * 1000000.0f);
    hash = hash * 2654435761u + (uint32_t)iteration;
    hash = (hash ^ (hash >> 16)) * 0x85ebca6b;
    hash = (hash ^ (hash >> 13)) * 0xc2b2ae35;
    hash = hash ^ (hash >> 16);
    return (float)(hash % 10000) / 10000.0f;
}

void ECMLocomotionSystem::initialize(components::ECMLocomotion& locomotion) {
    // NOTE: Initialization is now done in World.cpp using the microbe's unique seed
    // This function is kept for compatibility but should not be called directly
    // Each amoeba gets unique random values based on its seed field
    locomotion.targetVertexIndex = 0;

    // Set phase flags based on initial phase
    locomotion.isExtending = (locomotion.phase < EXTEND_PHASE);
    locomotion.isSearching = (locomotion.phase >= EXTEND_PHASE && locomotion.phase < SEARCH_PHASE);
    locomotion.isRetracting = (locomotion.phase >= SEARCH_PHASE);
}

void ECMLocomotionSystem::update(
    components::Microbe& microbe,
    components::InternalSkeleton& skeleton,
    PhysicsSystemState* physics,
    float dt
) {
    components::ECMLocomotion& loco = microbe.locomotion;

    // Update cycle phase (0-1 over 12 seconds)
    loco.phase += dt / CYCLE_DURATION;
    if (loco.phase >= 1.0f) {
        loco.phase -= 1.0f;

        // Increment cycle counter for new random values
        static int cycleCounter = 0;
        cycleCounter++;

        // Choose new pseudopod target vertex when cycle resets
        int vertexCount = microbe.softBody.vertexCount;
        if (vertexCount > 0) {
            // Pick a random vertex using microbe's unique seed
            float vertexRand = hashToFloat(microbe.stats.seed, cycleCounter * 2);
            loco.targetVertexIndex = (int)(vertexRand * vertexCount) % vertexCount;

            // Random direction in XZ plane using microbe's unique seed
            float angle = hashToFloat(microbe.stats.seed, cycleCounter * 2 + 1) * 2.0f * PI;
            loco.targetDirection = {cosf(angle), 0.0f, sinf(angle)};
        }
    }

    // Update phase flags
    loco.isExtending = (loco.phase < EXTEND_PHASE);
    loco.isSearching = (loco.phase >= EXTEND_PHASE && loco.phase < SEARCH_PHASE);
    loco.isRetracting = (loco.phase >= SEARCH_PHASE);

    // Apply forces based on current phase (to skeleton rigid bodies)
    if (loco.isExtending) {
        applyExtensionForces(microbe, skeleton, physics, dt);
    } else if (loco.isSearching) {
        applySearchForces(microbe, skeleton, physics, dt);
    } else if (loco.isRetracting) {
        applyRetractionForces(microbe, physics, dt);
    }

    // Update wiggle phase for lateral motion
    loco.wigglePhase += dt * 2.0f; // Wiggle frequency
}

void ECMLocomotionSystem::applyExtensionForces(
    components::Microbe& microbe,
    components::InternalSkeleton& skeleton,
    PhysicsSystemState* physics,
    float dt
) {
    // Apply extension force to skeleton rigid bodies (Internal Motor model)
    // Skeleton moves forward → hits interior of Skin → Skin stretches forward
    if (skeleton.skeletonBodyIDs.empty()) return;

    JPH::BodyInterface& bodyInterface = physics->physicsSystem->GetBodyInterface();

    // Calculate force in target direction (horizontal only)
    Vector3 dir = microbe.locomotion.targetDirection;
    JPH::Vec3 force(
        dir.x * PSEUDOPOD_EXTENSION_FORCE,
        0.0f, // No vertical force
        dir.z * PSEUDOPOD_EXTENSION_FORCE
    );

    // Apply force to all skeleton nodes (or select one based on targetVertexIndex)
    int targetSkeletonIdx = microbe.locomotion.targetVertexIndex % (int)skeleton.skeletonBodyIDs.size();
    JPH::BodyID skeletonBodyID = skeleton.skeletonBodyIDs[targetSkeletonIdx];

    if (!skeletonBodyID.IsInvalid()) {
        // Apply force at center of mass (skeleton node moves forward)
        bodyInterface.AddForce(skeletonBodyID, force);
    }
}

void ECMLocomotionSystem::applySearchForces(
    components::Microbe& microbe,
    components::InternalSkeleton& skeleton,
    PhysicsSystemState* physics,
    float dt
) {
    // Apply lateral wiggle to skeleton rigid bodies creating zig-zag motion
    if (skeleton.skeletonBodyIDs.empty()) return;

    JPH::BodyInterface& bodyInterface = physics->physicsSystem->GetBodyInterface();

    // Wiggle perpendicular to target direction
    Vector3 dir = microbe.locomotion.targetDirection;
    float wiggle = sinf(microbe.locomotion.wigglePhase);

    // Perpendicular direction in XZ plane
    Vector3 perpDir = {-dir.z, 0.0f, dir.x};

    // Calculate lateral wiggle force
    JPH::Vec3 force(
        perpDir.x * WIGGLE_FORCE * wiggle,
        0.0f,
        perpDir.z * WIGGLE_FORCE * wiggle
    );

    // Apply to target skeleton node
    int targetSkeletonIdx = microbe.locomotion.targetVertexIndex % (int)skeleton.skeletonBodyIDs.size();
    JPH::BodyID skeletonBodyID = skeleton.skeletonBodyIDs[targetSkeletonIdx];

    if (!skeletonBodyID.IsInvalid()) {
        bodyInterface.AddForce(skeletonBodyID, force);
    }
}

void ECMLocomotionSystem::applyRetractionForces(
    components::Microbe& microbe,
    PhysicsSystemState* physics,
    float dt
) {
    int targetIdx = microbe.locomotion.targetVertexIndex;
    JPH::BodyID bodyID = microbe.softBody.bodyID;
    if (bodyID.IsInvalid()) return;

    JPH::BodyLockWrite lock(physics->physicsSystem->GetBodyLockInterface(), bodyID);
    if (!lock.Succeeded()) return;

    JPH::Body& body = lock.GetBody();
    JPH::SoftBodyMotionProperties* motionProps =
        static_cast<JPH::SoftBodyMotionProperties*>(body.GetMotionProperties());
    if (motionProps == nullptr) return;

    JPH::Array<JPH::SoftBodyVertex>& vertices = motionProps->GetVertices();
    if (targetIdx >= (int)vertices.size()) return;

    // 1. Calculate direction from Center-of-Mass to Target Vertex
    JPH::RMat44 comTransform = JPH::RMat44::sRotationTranslation(body.GetRotation(), body.GetCenterOfMassPosition());
    JPH::Vec3 worldPosVertex = comTransform * vertices[targetIdx].mPosition;
    JPH::Vec3 center = body.GetCenterOfMassPosition();

    JPH::Vec3 pullDir = worldPosVertex - center;
    float dist = pullDir.Length();
    if (dist < 0.001f) return;
    pullDir /= dist;

    // 2. Normalize Force: Prevent "Force Explosion" by dividing by vertex count
    float vertexCount = (float)vertices.size();
    if (vertexCount <= 1.0f) return;

    float forcePerVertex = RETRACT_FORCE / (vertexCount - 1.0f);
    float velocityMagnitude = forcePerVertex * dt * 5.0f; // 5x multiplier to overcome damping
    JPH::Vec3 velocityImpulse = pullDir * velocityMagnitude;

    for (size_t i = 0; i < vertices.size(); ++i) {
        if ((int)i == targetIdx) continue; // Don't move the foot (anchor)
        vertices[i].mVelocity += velocityImpulse;
    }
}

} // namespace micro_idle
