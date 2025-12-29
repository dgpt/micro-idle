#include "ECMLocomotionSystem.h"
#include <cmath>
#include <stdio.h>
#include <cstdint>
#include <Jolt/Physics/SoftBody/SoftBodyMotionProperties.h>
#include <Jolt/Physics/Body/BodyLockInterface.h>
#include <Jolt/Physics/Body/Body.h>

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

    // Apply forces based on current phase
    if (loco.isExtending) {
        applyExtensionForces(microbe, physics, dt);
    } else if (loco.isSearching) {
        applySearchForces(microbe, physics, dt);
    } else if (loco.isRetracting) {
        applyRetractionForces(microbe, physics, dt);
    }

    // Update wiggle phase for lateral motion
    loco.wigglePhase += dt * 2.0f; // Wiggle frequency
}

void ECMLocomotionSystem::applyExtensionForces(
    components::Microbe& microbe,
    PhysicsSystemState* physics,
    float dt
) {
    // Extend one soft body vertex as pseudopod in target direction
    int targetIdx = microbe.locomotion.targetVertexIndex;
    if (targetIdx >= microbe.softBody.vertexCount) return;

    JPH::BodyID bodyID = microbe.softBody.bodyID;
    if (bodyID.IsInvalid()) return;

    // Calculate velocity change in target direction (horizontal only)
    Vector3 dir = microbe.locomotion.targetDirection;
    float velocityMagnitude = PSEUDOPOD_EXTENSION_FORCE * dt; // Convert force to velocity impulse
    JPH::Vec3 velocityDelta(
        dir.x * velocityMagnitude,
        0.0f, // No vertical velocity
        dir.z * velocityMagnitude
    );

    // Directly modify vertex velocity
    JPH::BodyLockWrite lock(physics->physicsSystem->GetBodyLockInterface(), bodyID);
    if (lock.Succeeded()) {
        JPH::Body& body = lock.GetBody();
        JPH::SoftBodyMotionProperties* motionProps =
            static_cast<JPH::SoftBodyMotionProperties*>(body.GetMotionProperties());

        if (motionProps != nullptr) {
            JPH::Array<JPH::SoftBodyVertex>& vertices = motionProps->GetVertices();
            if (targetIdx < (int)vertices.size()) {
                // Add velocity to the target vertex
                vertices[targetIdx].mVelocity += velocityDelta;
            }
        }
    }
}

void ECMLocomotionSystem::applySearchForces(
    components::Microbe& microbe,
    PhysicsSystemState* physics,
    float dt
) {
    // Apply lateral wiggle to soft body vertex creating zig-zag motion
    int targetIdx = microbe.locomotion.targetVertexIndex;
    if (targetIdx >= microbe.softBody.vertexCount) return;

    JPH::BodyID bodyID = microbe.softBody.bodyID;
    if (bodyID.IsInvalid()) return;

    // Wiggle perpendicular to target direction
    Vector3 dir = microbe.locomotion.targetDirection;
    float wiggle = sinf(microbe.locomotion.wigglePhase);

    // Perpendicular direction in XZ plane
    Vector3 perpDir = {-dir.z, 0.0f, dir.x};

    // Calculate lateral wiggle velocity
    float velocityMagnitude = WIGGLE_FORCE * dt * wiggle;
    JPH::Vec3 velocityDelta(
        perpDir.x * velocityMagnitude,
        0.0f,
        perpDir.z * velocityMagnitude
    );

    // Directly modify vertex velocity
    JPH::BodyLockWrite lock(physics->physicsSystem->GetBodyLockInterface(), bodyID);
    if (lock.Succeeded()) {
        JPH::Body& body = lock.GetBody();
        JPH::SoftBodyMotionProperties* motionProps =
            static_cast<JPH::SoftBodyMotionProperties*>(body.GetMotionProperties());

        if (motionProps != nullptr) {
            JPH::Array<JPH::SoftBodyVertex>& vertices = motionProps->GetVertices();
            if (targetIdx < (int)vertices.size()) {
                vertices[targetIdx].mVelocity += velocityDelta;
            }
        }
    }
}

void ECMLocomotionSystem::applyRetractionForces(
    components::Microbe& microbe,
    PhysicsSystemState* physics,
    float dt
) {
    // Pull target vertex back toward soft body center
    int targetIdx = microbe.locomotion.targetVertexIndex;
    if (targetIdx >= microbe.softBody.vertexCount) return;

    JPH::BodyID bodyID = microbe.softBody.bodyID;
    if (bodyID.IsInvalid()) return;

    JPH::BodyLockWrite lock(physics->physicsSystem->GetBodyLockInterface(), bodyID);
    if (!lock.Succeeded()) return;

    JPH::Body& body = lock.GetBody();
    JPH::SoftBodyMotionProperties* motionProps =
        static_cast<JPH::SoftBodyMotionProperties*>(body.GetMotionProperties());

    if (motionProps == nullptr) return;

    // Get soft body center of mass
    JPH::Vec3 center = body.GetCenterOfMassPosition();

    // Get target vertex position (in world space)
    JPH::Array<JPH::SoftBodyVertex>& vertices = motionProps->GetVertices();
    if (targetIdx >= (int)vertices.size()) return;

    JPH::Vec3 localPos = vertices[targetIdx].mPosition;
    JPH::RMat44 comTransform = JPH::RMat44::sRotationTranslation(body.GetRotation(), body.GetCenterOfMassPosition());
    JPH::Vec3 worldPos = comTransform * localPos;

    // Direction from vertex to center
    JPH::Vec3 toCenter = center - worldPos;

    // Normalize and calculate retraction velocity
    float length = toCenter.Length();
    if (length > 0.001f) {
        toCenter /= length;

        // Calculate velocity toward center (convert force to velocity impulse)
        float velocityMagnitude = RETRACT_FORCE * dt;
        JPH::Vec3 velocityDelta = toCenter * velocityMagnitude;

        // Add velocity to vertex
        vertices[targetIdx].mVelocity += velocityDelta;
    }
}

} // namespace micro_idle
