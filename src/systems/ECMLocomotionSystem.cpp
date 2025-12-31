#include "ECMLocomotionSystem.h"
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <cstdint>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/SoftBody/SoftBodyMotionProperties.h>

namespace micro_idle {

static float rand01() {
    return (float)rand() / (float)RAND_MAX;
}

static float wrapAngle(float angle) {
    if (angle > PI) {
        angle = fmodf(angle + PI, 2.0f * PI) - PI;
    } else if (angle < -PI) {
        angle = fmodf(angle - PI, 2.0f * PI) + PI;
    }
    return angle;
}

static float hashToFloat(float seed, int iteration) {
    uint32_t hash = (uint32_t)(seed * 1000000.0f);
    hash = hash * 2654435761u + (uint32_t)iteration;
    hash = (hash ^ (hash >> 16)) * 0x85ebca6b;
    hash = (hash ^ (hash >> 13)) * 0xc2b2ae35;
    hash = hash ^ (hash >> 16);
    return (float)(hash % 10000) / 10000.0f;
}

void ECMLocomotionSystem::initialize(components::ECMLocomotion& locomotion, float seed) {
    float baseMemory = K0 * TAU_M;
    for (int i = 0; i < CortexSamples; i++) {
        locomotion.memory[i] = baseMemory;
        locomotion.inhibitor[i] = 0.0f;
    }

    locomotion.activeIndex = -1;
    locomotion.activeTime = 0.0f;
    locomotion.activeDuration = 0.0f;
    locomotion.idleTime = START_COOLDOWN;

    locomotion.lastAngle = hashToFloat(seed, 0) * 2.0f * PI;
    locomotion.activeAngle = locomotion.lastAngle;
    locomotion.zigzagSign = hashToFloat(seed, 1) < 0.5f ? -1 : 1;

    locomotion.targetDirection = {cosf(locomotion.lastAngle), 0.0f, sinf(locomotion.lastAngle)};
}

void ECMLocomotionSystem::registerSystem(flecs::world& world, PhysicsSystemState* physics) {
    world.system<components::Microbe, components::ECMLocomotion>("ECMLocomotionSystem")
        .kind(flecs::OnUpdate)
        .run([physics](flecs::iter& it) {
            while (it.next()) {
                auto microbes = it.field<components::Microbe>(0);
                auto locomotions = it.field<components::ECMLocomotion>(1);

                for (auto i : it) {
                    update(it.entity(i), microbes[i], locomotions[i], physics, it.delta_time());
                }
            }
        });
}

void ECMLocomotionSystem::update(
    flecs::entity e,
    components::Microbe& microbe,
    components::ECMLocomotion& locomotion,
    PhysicsSystemState* physics,
    float dt
) {
    (void)e;
    (void)microbe;
    stepCortex(locomotion, dt);

    if (locomotion.activeIndex >= 0) {
        locomotion.activeTime += dt;
        applyPseudopodForces(locomotion, microbe, physics, dt);

        if (shouldStopPseudopod(locomotion, dt)) {
            locomotion.lastAngle = locomotion.activeAngle;
            locomotion.activeIndex = -1;
            locomotion.activeTime = 0.0f;
            locomotion.activeDuration = 0.0f;
            locomotion.idleTime = 0.0f;
            locomotion.zigzagSign = -locomotion.zigzagSign;
        }
        return;
    }

    locomotion.idleTime += dt;
    if (locomotion.idleTime < START_COOLDOWN) {
        return;
    }

    if (tryStartPseudopod(locomotion, dt)) {
        locomotion.targetDirection = {cosf(locomotion.activeAngle), 0.0f, sinf(locomotion.activeAngle)};
        locomotion.idleTime = 0.0f;
    }
}

void ECMLocomotionSystem::stepCortex(components::ECMLocomotion& locomotion, float dt) {
    float nextMemory[CortexSamples];
    float nextInhibitor[CortexSamples];

    for (int i = 0; i < CortexSamples; i++) {
        int left = (i - 1 + CortexSamples) % CortexSamples;
        int right = (i + 1) % CortexSamples;

        float lapM = locomotion.memory[left] + locomotion.memory[right] - 2.0f * locomotion.memory[i];
        float lapL = locomotion.inhibitor[left] + locomotion.inhibitor[right] - 2.0f * locomotion.inhibitor[i];

        float source = 0.0f;
        if (locomotion.activeIndex >= 0) {
            int dist = std::abs(i - locomotion.activeIndex);
            dist = std::min(dist, CortexSamples - dist);
            if (dist == 0) {
                source = 1.0f;
            } else if (dist == 1) {
                source = 0.6f;
            }
        }

        float mem = locomotion.memory[i] + dt * (K0 + K1 * source - locomotion.memory[i] / TAU_M + D_M * lapM);
        float inh = locomotion.inhibitor[i] + dt * (K_L * source - locomotion.inhibitor[i] / TAU_L + D_L * lapL);

        nextMemory[i] = std::max(0.0f, mem);
        nextInhibitor[i] = std::max(0.0f, inh);
    }

    for (int i = 0; i < CortexSamples; i++) {
        locomotion.memory[i] = nextMemory[i];
        locomotion.inhibitor[i] = nextInhibitor[i];
    }
}

bool ECMLocomotionSystem::shouldStopPseudopod(const components::ECMLocomotion& locomotion, float dt) {
    if (locomotion.activeIndex < 0) {
        return false;
    }

    if (locomotion.activeTime < MIN_PSEUDOPOD_DURATION) {
        return false;
    }

    if (locomotion.activeTime >= locomotion.activeDuration) {
        return true;
    }

    float localInhibitor = locomotion.inhibitor[locomotion.activeIndex];
    float stopRate = MU * localInhibitor * localInhibitor * localInhibitor;
    return rand01() < stopRate * dt;
}

bool ECMLocomotionSystem::tryStartPseudopod(components::ECMLocomotion& locomotion, float dt) {
    float rates[CortexSamples];
    float totalRate = 0.0f;

    for (int i = 0; i < CortexSamples; i++) {
        float mem = locomotion.memory[i];
        float inh = locomotion.inhibitor[i];

        float rate = ECM_EPSILON * mem * mem * mem / (1.0f + A * inh);

        float angle = (2.0f * PI * (float)i) / (float)CortexSamples;
        float side = sinf(angle - locomotion.lastAngle);
        float zigzagBias = 1.0f + ZIGZAG_STRENGTH * (float)locomotion.zigzagSign * side;
        rate *= std::max(0.1f, zigzagBias);

        rates[i] = rate;
        totalRate += rate;
    }

    if (totalRate <= 0.0f) {
        return false;
    }

    float startChance = totalRate * dt;
    if (rand01() >= std::min(1.0f, startChance)) {
        return false;
    }

    float pick = rand01() * totalRate;
    float accum = 0.0f;
    int chosen = 0;
    for (int i = 0; i < CortexSamples; i++) {
        accum += rates[i];
        if (pick <= accum) {
            chosen = i;
            break;
        }
    }

    locomotion.activeIndex = chosen;
    locomotion.activeTime = 0.0f;
    locomotion.activeDuration = MIN_PSEUDOPOD_DURATION +
        rand01() * (MAX_PSEUDOPOD_DURATION - MIN_PSEUDOPOD_DURATION);
    locomotion.activeAngle = (2.0f * PI * (float)chosen) / (float)CortexSamples;

    return true;
}

void ECMLocomotionSystem::applyPseudopodForces(
    components::ECMLocomotion& locomotion,
    components::Microbe& microbe,
    PhysicsSystemState* physics,
    float dt
) {
    if (locomotion.activeIndex < 0 || microbe.softBody.bodyID.IsInvalid()) {
        return;
    }

    float rampIn = std::min(1.0f, locomotion.activeTime / FORCE_RAMP_TIME);
    float remaining = std::max(0.0f, locomotion.activeDuration - locomotion.activeTime);
    float rampOut = std::min(1.0f, remaining / FORCE_RAMP_TIME);
    float ramp = std::min(rampIn, rampOut);

    Vector3 dir = locomotion.targetDirection;
    float dirLenSq = dir.x * dir.x + dir.z * dir.z;
    if (dirLenSq < 0.0001f) {
        return;
    }

    JPH::BodyLockWrite lock(physics->physicsSystem->GetBodyLockInterface(), microbe.softBody.bodyID);
    if (!lock.Succeeded()) {
        return;
    }

    JPH::Body& body = lock.GetBody();
    JPH::SoftBodyMotionProperties* motionProps =
        static_cast<JPH::SoftBodyMotionProperties*>(body.GetMotionProperties());
    if (!motionProps) {
        return;
    }

    JPH::Array<JPH::SoftBodyVertex>& vertices = motionProps->GetVertices();
    if (vertices.empty()) {
        return;
    }

    JPH::Vec3 worldDir(dir.x, 0.0f, dir.z);
    worldDir = worldDir.Normalized();
    JPH::Quat invRot = body.GetRotation().Conjugated();
    JPH::Vec3 localDir = invRot * worldDir;
    float targetAngle = atan2f(localDir.GetZ(), localDir.GetX());

    float minRadius = microbe.stats.baseRadius * 0.55f;
    float maxRadius = microbe.stats.baseRadius * 1.0f;
    float arc = PI / 5.0f;
    float denom = std::max(0.001f, maxRadius - minRadius);

    float impulse = FORCE_MAGNITUDE * ramp * dt;
    float contractionImpulse = CONTRACTION_MAGNITUDE * ramp * dt;
    float rearAngle = wrapAngle(targetAngle + PI);
    body.AddForce(worldDir * (BODY_FORCE * microbe.stats.baseRadius * ramp));

    for (auto& vertex : vertices) {
        const JPH::Vec3& p = vertex.mPosition;
        float radial = sqrtf(p.GetX() * p.GetX() + p.GetZ() * p.GetZ());
        if (radial < minRadius) {
            continue;
        }

        float angle = atan2f(p.GetZ(), p.GetX());
        float radiusWeight = std::clamp((radial - minRadius) / denom, 0.0f, 1.0f);
        if (radiusWeight <= 0.0f) {
            continue;
        }

        float frontDelta = wrapAngle(angle - targetAngle);
        float frontAbs = fabsf(frontDelta);
        if (frontAbs <= arc) {
            float angleWeight = cosf((frontAbs / arc) * (PI * 0.5f));
            float weight = angleWeight * radiusWeight;
            if (weight > 0.0f) {
                vertex.mVelocity += localDir * (impulse * weight);
            }
        }

        float rearDelta = wrapAngle(angle - rearAngle);
        float rearAbs = fabsf(rearDelta);
        if (rearAbs <= arc) {
            float angleWeight = cosf((rearAbs / arc) * (PI * 0.5f));
            float weight = angleWeight * radiusWeight;
            if (weight > 0.0f) {
                vertex.mVelocity -= localDir * (contractionImpulse * weight);
            }
        }
    }
}

} // namespace micro_idle
