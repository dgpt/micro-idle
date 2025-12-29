#include "ECMLocomotionSystem.h"
#include <cmath>
#include <stdio.h>

namespace micro_idle {

void ECMLocomotionSystem::initialize(components::ECMLocomotion& locomotion) {
    // Start at random phase so amoebas don't all look the same initially
    locomotion.phase = (float)rand() / RAND_MAX;
    locomotion.targetParticleIndex = 0;

    // Random initial direction
    float angle = (float)rand() / RAND_MAX * 2.0f * PI;
    locomotion.targetDirection = {cosf(angle), 0.0f, sinf(angle)};

    locomotion.wigglePhase = (float)rand() / RAND_MAX * 2.0f * PI;

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

        // Choose new pseudopod target particle when cycle resets
        int particleCount = (int)microbe.softBody.particleBodyIDs.size();
        if (particleCount > 0) {
            // Pick a membrane vertex
            int membraneStart = microbe.softBody.membrane.meshVertexStartIndex;
            int membraneCount = microbe.softBody.membrane.meshVertexCount;
            if (membraneCount > 0) {
                loco.targetParticleIndex = membraneStart + (rand() % membraneCount);
            } else {
                loco.targetParticleIndex = 0;
            }

            // Random direction in XZ plane (horizontal movement)
            float angle = (float)rand() / RAND_MAX * 2.0f * PI;
            loco.targetDirection = {cosf(angle), 0.0f, sinf(angle)};
        }
    }

    // Update phase flags
    loco.isExtending = (loco.phase < EXTEND_PHASE);
    loco.isSearching = (loco.phase >= EXTEND_PHASE && loco.phase < SEARCH_PHASE);
    loco.isRetracting = (loco.phase >= SEARCH_PHASE);

    // Apply forces based on current phase
    if (loco.isExtending) {
        applyExtensionForces(microbe, physics);
    } else if (loco.isSearching) {
        applySearchForces(microbe, physics);
    } else if (loco.isRetracting) {
        applyRetractionForces(microbe, physics);
    }

    // Update wiggle phase for lateral motion
    loco.wigglePhase += dt * 2.0f; // Wiggle frequency
}

void ECMLocomotionSystem::applyExtensionForces(
    components::Microbe& microbe,
    PhysicsSystemState* physics
) {
    // Extend one thin pseudopod in target direction
    if (microbe.softBody.particleBodyIDs.empty()) return;

    int targetIdx = microbe.locomotion.targetParticleIndex;
    if (targetIdx >= (int)microbe.softBody.particleBodyIDs.size()) return;

    JPH::BodyInterface& bodyInterface = physics->physicsSystem->GetBodyInterface();
    JPH::BodyID bodyID = microbe.softBody.particleBodyIDs[targetIdx];

    // Apply force in target direction
    Vector3 dir = microbe.locomotion.targetDirection;
    JPH::Vec3 force(dir.x * PSEUDOPOD_EXTENSION_FORCE,
                    0.0f, // Don't apply vertical force
                    dir.z * PSEUDOPOD_EXTENSION_FORCE);

    bodyInterface.AddForce(bodyID, force);
}

void ECMLocomotionSystem::applySearchForces(
    components::Microbe& microbe,
    PhysicsSystemState* physics
) {
    // Apply lateral wiggle creating zig-zag motion
    if (microbe.softBody.particleBodyIDs.empty()) return;

    int targetIdx = microbe.locomotion.targetParticleIndex;
    if (targetIdx >= (int)microbe.softBody.particleBodyIDs.size()) return;

    JPH::BodyInterface& bodyInterface = physics->physicsSystem->GetBodyInterface();
    JPH::BodyID bodyID = microbe.softBody.particleBodyIDs[targetIdx];

    // Wiggle perpendicular to target direction
    Vector3 dir = microbe.locomotion.targetDirection;
    float wiggle = sinf(microbe.locomotion.wigglePhase);

    // Perpendicular direction in XZ plane
    Vector3 perpDir = {-dir.z, 0.0f, dir.x};

    JPH::Vec3 force(perpDir.x * WIGGLE_FORCE * wiggle,
                    0.0f,
                    perpDir.z * WIGGLE_FORCE * wiggle);

    bodyInterface.AddForce(bodyID, force);
}

void ECMLocomotionSystem::applyRetractionForces(
    components::Microbe& microbe,
    PhysicsSystemState* physics
) {
    // Pull pseudopod back toward center
    if (microbe.softBody.particleBodyIDs.empty()) return;

    int targetIdx = microbe.locomotion.targetParticleIndex;
    if (targetIdx >= (int)microbe.softBody.particleBodyIDs.size()) return;

    JPH::BodyInterface& bodyInterface = physics->physicsSystem->GetBodyInterface();
    JPH::BodyID bodyID = microbe.softBody.particleBodyIDs[targetIdx];

    // Calculate center of mass
    Vector3 center = {0, 0, 0};
    int count = 0;
    for (JPH::BodyID bid : microbe.softBody.particleBodyIDs) {
        JPH::RVec3 pos = bodyInterface.GetPosition(bid);
        center.x += (float)pos.GetX();
        center.y += (float)pos.GetY();
        center.z += (float)pos.GetZ();
        count++;
    }
    if (count > 0) {
        center.x /= count;
        center.y /= count;
        center.z /= count;
    }

    // Direction from particle to center
    JPH::RVec3 particlePos = bodyInterface.GetPosition(bodyID);
    Vector3 toCenter = {
        center.x - (float)particlePos.GetX(),
        center.y - (float)particlePos.GetY(),
        center.z - (float)particlePos.GetZ()
    };

    // Normalize and apply retraction force
    float length = sqrtf(toCenter.x*toCenter.x + toCenter.y*toCenter.y + toCenter.z*toCenter.z);
    if (length > 0.001f) {
        toCenter.x /= length;
        toCenter.y /= length;
        toCenter.z /= length;

        JPH::Vec3 force(toCenter.x * RETRACT_FORCE,
                        toCenter.y * RETRACT_FORCE,
                        toCenter.z * RETRACT_FORCE);

        bodyInterface.AddForce(bodyID, force);
    }
}

} // namespace micro_idle
