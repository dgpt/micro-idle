#include "ECMLocomotionSystem.h"
#include "src/components/Input.h"
#include "src/components/Transform.h"
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <cstdint>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/SoftBody/SoftBodyMotionProperties.h>

namespace micro_idle {

constexpr int POD_INACTIVE = 0;
constexpr int POD_EXTEND = 1;
constexpr int POD_HOLD = 2;
constexpr int POD_RETRACT = 3;

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

static bool computeDesiredAngle(
    flecs::entity e,
    const components::Transform& transform,
    const components::Microbe& microbe,
    const components::ECMLocomotion& locomotion,
    float* outAngle
) {
    auto input = e.world().get<components::InputState>();
    if (!input || !input->mouseWorldValid) {
        return false;
    }

    float dx = transform.position.x - input->mouseWorld.x;
    float dz = transform.position.z - input->mouseWorld.z;
    float distSq = dx * dx + dz * dz;
    if (distSq < 0.0001f) {
        return false;
    }

    float dist = sqrtf(distSq);
    float orbitRadius = std::max(2.0f, microbe.stats.baseRadius * 8.0f);
    float invDist = 1.0f / dist;
    float rx = dx * invDist;
    float rz = dz * invDist;
    float tx = -rz * (float)locomotion.orbitSign;
    float tz = rx * (float)locomotion.orbitSign;

    float avoid = std::clamp((orbitRadius - dist) / orbitRadius, 0.0f, 1.0f);
    float seek = std::clamp((dist - orbitRadius) / orbitRadius, 0.0f, 1.0f) * 0.25f;
    float dirX = tx + rx * (avoid * 1.5f - seek);
    float dirZ = tz + rz * (avoid * 1.5f - seek);

    float lenSq = dirX * dirX + dirZ * dirZ;
    if (lenSq < 0.0001f) {
        dirX = tx;
        dirZ = tz;
        lenSq = dirX * dirX + dirZ * dirZ;
        if (lenSq < 0.0001f) {
            return false;
        }
    }
    float invLen = 1.0f / sqrtf(lenSq);
    dirX *= invLen;
    dirZ *= invLen;

    *outAngle = atan2f(dirZ, dirX);
    return true;
}

void ECMLocomotionSystem::initialize(components::ECMLocomotion& locomotion, float seed) {
    float baseMemory = K0 * TAU_M;
    for (int i = 0; i < CortexSamples; i++) {
        locomotion.memory[i] = baseMemory;
        locomotion.inhibitor[i] = 0.0f;
    }

    for (int i = 0; i < components::ECMLocomotion::MaxPods; i++) {
        locomotion.pods[i] = components::ECMLocomotion::Pod{};
        locomotion.pods[i].index = -1;
        locomotion.pods[i].state = POD_INACTIVE;
    }
    locomotion.idleTime = START_COOLDOWN;

    locomotion.lastAngle = hashToFloat(seed, 0) * 2.0f * PI;
    locomotion.zigzagSign = hashToFloat(seed, 1) < 0.5f ? -1 : 1;
    locomotion.orbitSign = hashToFloat(seed, 2) < 0.5f ? -1 : 1;

    locomotion.targetDirection = {cosf(locomotion.lastAngle), 0.0f, sinf(locomotion.lastAngle)};
}

void ECMLocomotionSystem::registerSystem(flecs::world& world, PhysicsSystemState* physics) {
    world.system<components::Microbe, components::ECMLocomotion, components::Transform>("ECMLocomotionSystem")
        .kind(flecs::OnUpdate)
        .run([physics](flecs::iter& it) {
            while (it.next()) {
                auto microbes = it.field<components::Microbe>(0);
                auto locomotions = it.field<components::ECMLocomotion>(1);
                auto transforms = it.field<components::Transform>(2);

                for (auto i : it) {
                    update(it.entity(i), microbes[i], locomotions[i], transforms[i], physics, it.delta_time());
                }
            }
        });
}

void ECMLocomotionSystem::update(
    flecs::entity e,
    components::Microbe& microbe,
    components::ECMLocomotion& locomotion,
    components::Transform& transform,
    PhysicsSystemState* physics,
    float dt
) {
    stepCortex(locomotion, dt);

    int extendCount = 0;
    int retractCount = 0;
    int activeCount = 0;
    for (int i = 0; i < components::ECMLocomotion::MaxPods; i++) {
        const auto& pod = locomotion.pods[i];
        extendCount += pod.state == POD_EXTEND ? 1 : 0;
        retractCount += pod.state == POD_RETRACT ? 1 : 0;
        activeCount += pod.state == POD_INACTIVE ? 0 : 1;
    }

    for (int i = 0; i < components::ECMLocomotion::MaxPods; i++) {
        auto& pod = locomotion.pods[i];
        if (pod.state != POD_INACTIVE) {
            pod.time += dt;
        }
    }

    applyPseudopodForces(locomotion, microbe, physics, dt);

    for (int i = 0; i < components::ECMLocomotion::MaxPods; i++) {
        auto& pod = locomotion.pods[i];
        if (pod.state == POD_EXTEND && pod.time >= pod.duration) {
            pod.state = POD_HOLD;
            pod.time = 0.0f;
            pod.duration = HOLD_DURATION;
            locomotion.lastAngle = pod.angle;
            locomotion.idleTime = 0.0f;
            locomotion.zigzagSign = -locomotion.zigzagSign;
        }
    }

    for (int i = 0; i < components::ECMLocomotion::MaxPods; i++) {
        auto& pod = locomotion.pods[i];
        if (pod.state == POD_RETRACT && pod.time >= pod.duration) {
            pod = components::ECMLocomotion::Pod{};
            pod.index = -1;
            pod.state = POD_INACTIVE;
        }
    }

    extendCount = 0;
    retractCount = 0;
    activeCount = 0;
    for (int i = 0; i < components::ECMLocomotion::MaxPods; i++) {
        const auto& pod = locomotion.pods[i];
        extendCount += pod.state == POD_EXTEND ? 1 : 0;
        retractCount += pod.state == POD_RETRACT ? 1 : 0;
        activeCount += pod.state == POD_INACTIVE ? 0 : 1;
    }

    int desiredActivePods = std::min(components::ECMLocomotion::MaxPods, 3);
    if (retractCount == 0) {
        int retractCandidate = -1;
        float oldestHold = -1.0f;
        for (int i = 0; i < components::ECMLocomotion::MaxPods; i++) {
            auto& pod = locomotion.pods[i];
            if (pod.state == POD_HOLD && pod.time >= pod.duration && pod.time > oldestHold) {
                oldestHold = pod.time;
                retractCandidate = i;
            }
        }
        if (retractCandidate >= 0 && (activeCount > desiredActivePods || extendCount == 0)) {
            auto& pod = locomotion.pods[retractCandidate];
            pod.state = POD_RETRACT;
            pod.time = 0.0f;
            pod.duration = RETRACT_DURATION;
            retractCount = 1;
            activeCount -= 1;
        }
    }

    if (retractCount == 0 && activeCount < desiredActivePods) {
        locomotion.idleTime += dt;
        if (locomotion.idleTime >= START_COOLDOWN) {
            float desiredAngle = 0.0f;
            bool hasDesired = computeDesiredAngle(e, transform, microbe, locomotion, &desiredAngle);
            int available = desiredActivePods - activeCount;
            float attemptDt = available > 0 ? (dt / (float)available) : dt;
            bool startedAny = false;
            for (int attempt = 0; attempt < available; attempt++) {
                int chosenIndex = -1;
                if (!tryStartPseudopod(locomotion, attemptDt, desiredAngle, hasDesired, &chosenIndex)) {
                    continue;
                }
                bool usedIndex = false;
                for (int i = 0; i < components::ECMLocomotion::MaxPods; i++) {
                    if (locomotion.pods[i].state != POD_INACTIVE && locomotion.pods[i].index == chosenIndex) {
                        usedIndex = true;
                        break;
                    }
                }
                if (usedIndex) {
                    continue;
                }
                int slot = -1;
                for (int i = 0; i < components::ECMLocomotion::MaxPods; i++) {
                    if (locomotion.pods[i].state == POD_INACTIVE) {
                        slot = i;
                        break;
                    }
                }
                if (slot < 0) {
                    break;
                }
                auto& pod = locomotion.pods[slot];
                pod.state = POD_EXTEND;
                pod.index = chosenIndex;
                pod.time = 0.0f;
                pod.duration = MIN_PSEUDOPOD_DURATION +
                    rand01() * (MAX_PSEUDOPOD_DURATION - MIN_PSEUDOPOD_DURATION);
                pod.angle = (2.0f * PI * (float)chosenIndex) / (float)CortexSamples;
                pod.extent = 0.0f;
                pod.anchorSet = false;
                pod.anchorLocal = {0.0f, 0.0f, 0.0f};
                float anchorOffset = microbe.stats.baseRadius * 0.5f;
                pod.anchorLocal = {cosf(pod.angle) * anchorOffset, 0.0f, sinf(pod.angle) * anchorOffset};
                pod.anchorSet = true;
                locomotion.targetDirection = {cosf(pod.angle), 0.0f, sinf(pod.angle)};
                startedAny = true;
                activeCount += 1;
            }
            if (startedAny) {
                locomotion.idleTime = 0.0f;
            }
        }
    } else if (retractCount > 0) {
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
        for (int p = 0; p < components::ECMLocomotion::MaxPods; p++) {
            const auto& pod = locomotion.pods[p];
            if (pod.state != POD_EXTEND && pod.state != POD_HOLD) {
                continue;
            }
            if (pod.index < 0) {
                continue;
            }
            int dist = std::abs(i - pod.index);
            dist = std::min(dist, CortexSamples - dist);
            if (dist == 0) {
                source = std::max(source, 1.0f);
            } else if (dist == 1) {
                source = std::max(source, 0.6f);
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

bool ECMLocomotionSystem::tryStartPseudopod(components::ECMLocomotion& locomotion, float dt, float desiredAngle, bool hasDesired, int* outIndex) {
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

        if (hasDesired) {
            float diff = wrapAngle(angle - desiredAngle);
            float align = cosf(diff);
            float desiredBias = 0.35f + 0.65f * std::max(0.0f, align);
            rate *= desiredBias;
        }

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

    if (outIndex == nullptr) {
        return false;
    }
    *outIndex = chosen;
    return true;
}

void ECMLocomotionSystem::applyPseudopodForces(
    components::ECMLocomotion& locomotion,
    components::Microbe& microbe,
    PhysicsSystemState* physics,
    float dt
) {
    if (microbe.softBody.bodyID.IsInvalid()) {
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

    JPH::Quat invRot = body.GetRotation().Conjugated();
    float minRadius = microbe.stats.baseRadius * 0.4f;
    float maxRadius = microbe.stats.baseRadius * 1.3f;
    float arc = PI / 4.0f;
    float denom = std::max(0.001f, maxRadius - minRadius);

    float vertexScale = std::max(0.9f, microbe.stats.baseRadius * 6.2f);
    float speed = body.GetLinearVelocity().Length();
    float maxVertexSpeed = vertexScale * 3.0f;
    float vertexSpeedScale = speed > maxVertexSpeed ? (maxVertexSpeed / speed) : 1.0f;
    float holdSpeedScale = std::max(0.6f, vertexSpeedScale);

    for (int i = 0; i < components::ECMLocomotion::MaxPods; i++) {
        auto& pod = locomotion.pods[i];
        if (pod.state == POD_INACTIVE) {
            continue;
        }
        JPH::Vec3 worldDir(cosf(pod.angle), 0.0f, sinf(pod.angle));
        JPH::Vec3 localDir = invRot * worldDir;
        float maxDot = 0.0f;
        for (const auto& vertex : vertices) {
            const JPH::Vec3& p = vertex.mPosition;
            float dot = p.GetX() * localDir.GetX() + p.GetZ() * localDir.GetZ();
            if (dot > maxDot) {
                maxDot = dot;
            }
        }
        if (!pod.anchorSet) {
            float anchorOffset = microbe.stats.baseRadius * 0.5f;
            pod.anchorLocal = {localDir.GetX() * anchorOffset, localDir.GetY() * anchorOffset, localDir.GetZ() * anchorOffset};
            pod.anchorSet = true;
        }
        if (pod.state == POD_RETRACT) {
            pod.extent = maxDot;
        } else {
            pod.extent = std::max(pod.extent, maxDot);
        }
    }

    auto applyPod = [&](float angle, float magnitude, bool inward) {
        if (magnitude <= 0.0f) {
            return;
        }
        JPH::Vec3 worldDir(cosf(angle), 0.0f, sinf(angle));
        JPH::Vec3 localDir = invRot * worldDir;
        float targetAngle = atan2f(localDir.GetZ(), localDir.GetX());

        for (auto& vertex : vertices) {
            const JPH::Vec3& p = vertex.mPosition;
            float radial = sqrtf(p.GetX() * p.GetX() + p.GetZ() * p.GetZ());
            if (radial < minRadius) {
                continue;
            }

            float angleAt = atan2f(p.GetZ(), p.GetX());
            float delta = wrapAngle(angleAt - targetAngle);
            float absDelta = fabsf(delta);
            if (absDelta > arc) {
                continue;
            }

            float radiusWeight = std::clamp((radial - minRadius) / denom, 0.0f, 1.0f);
            float angleWeight = cosf((absDelta / arc) * (PI * 0.5f));
            float weight = angleWeight * radiusWeight * radiusWeight;
            if (weight > 0.0f) {
                float sign = inward ? -1.0f : 1.0f;
                vertex.mVelocity += localDir * (magnitude * weight * sign);
            }
        }
    };

    for (int i = 0; i < components::ECMLocomotion::MaxPods; i++) {
        const auto& pod = locomotion.pods[i];
        if (pod.state == POD_INACTIVE) {
            continue;
        }

        float rampIn = std::min(1.0f, pod.time / FORCE_RAMP_TIME);
        float remaining = std::max(0.0f, pod.duration - pod.time);
        float rampOut = std::min(1.0f, remaining / FORCE_RAMP_TIME);
        float ramp = std::min(rampIn, rampOut);

        if (pod.state == POD_EXTEND) {
            float magnitude = FORCE_MAGNITUDE * vertexScale * ramp * vertexSpeedScale * dt;
            applyPod(pod.angle, magnitude, false);
        } else if (pod.state == POD_HOLD) {
            float magnitude = FORCE_MAGNITUDE * vertexScale * HOLD_FORCE_SCALE * holdSpeedScale * dt;
            applyPod(pod.angle, magnitude, false);
        } else if (pod.state == POD_RETRACT) {
            float magnitude = CONTRACTION_MAGNITUDE * vertexScale * RETRACT_FORCE_SCALE * ramp * vertexSpeedScale * dt;
            applyPod(pod.angle, magnitude, true);
        }
    }
}

} // namespace micro_idle
