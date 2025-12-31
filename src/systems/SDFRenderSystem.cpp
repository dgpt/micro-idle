#include "SDFRenderSystem.h"
#include "src/components/Microbe.h"
#include "src/components/ECMLocomotion.h"
#include "src/components/Transform.h"
#include "src/components/Rendering.h"
#include "src/rendering/RaymarchBounds.h"
#include "src/rendering/SDFShader.h"
#include "raylib.h"
#include "raymath.h"
#include <algorithm>
#include <cmath>

namespace micro_idle {

void SDFRenderSystem::registerSystem(flecs::world& world) {
    // System that renders microbes using SDF raymarching
    world.system<const components::Microbe, const components::ECMLocomotion, const components::Transform, const components::SDFRenderComponent>("SDFRenderSystem")
        .kind(flecs::PostUpdate)
        .each([&world](const components::Microbe& microbe,
                       const components::ECMLocomotion& locomotion,
                       const components::Transform& transform,
                       const components::SDFRenderComponent& sdf) {
            (void)transform;
            const auto* cameraState = world.get<components::CameraState>();
            if (!cameraState) {
                return;
            }

            if (sdf.vertexCount <= 0 || sdf.shader.id == 0) {
                return;
            }

            int count = sdf.vertexCount;
            if (count > 64) {
                count = 64;
            }

            rendering::SDFShaderUniforms uniforms;
            if (!rendering::initializeSDFUniforms(sdf.shader, uniforms)) {
                return;
            }

            rendering::setCameraPosition(sdf.shader, uniforms, cameraState->position);
            rendering::setTime(sdf.shader, uniforms, (float)GetTime());
            rendering::setMicrobeUniforms(
                sdf.shader,
                uniforms,
                count,
                microbe.stats.baseRadius,
                microbe.stats.color);
            rendering::setVertexPositions(
                sdf.shader,
                uniforms,
                sdf.vertexPositions,
                count);

            Vector3 minPos = sdf.vertexPositions[0];
            Vector3 maxPos = sdf.vertexPositions[0];
            for (int i = 1; i < count; i++) {
                const Vector3& p = sdf.vertexPositions[i];
                minPos.x = fminf(minPos.x, p.x);
                minPos.y = fminf(minPos.y, p.y);
                minPos.z = fminf(minPos.z, p.z);
                maxPos.x = fmaxf(maxPos.x, p.x);
                maxPos.y = fmaxf(maxPos.y, p.y);
                maxPos.z = fmaxf(maxPos.z, p.z);
            }

            Vector3 center = {
                (minPos.x + maxPos.x) * 0.5f,
                (minPos.y + maxPos.y) * 0.5f,
                (minPos.z + maxPos.z) * 0.5f
            };

            constexpr int kPodExtend = 1;
            constexpr int kPodHold = 2;
            Vector3 podDirs[components::ECMLocomotion::MaxPods];
            float podExtents[components::ECMLocomotion::MaxPods];
            Vector3 podAnchors[components::ECMLocomotion::MaxPods];
            int podCount = 0;

            auto addPod = [&](const components::ECMLocomotion::Pod& pod, float progress, float strengthScale) {
                if (podCount >= components::ECMLocomotion::MaxPods) {
                    return;
                }
                if (progress <= 0.0f) {
                    return;
                }
                Vector3 dir = {cosf(pod.angle), 0.0f, sinf(pod.angle)};
                float anchorOffset = microbe.stats.baseRadius * 0.25f;
                float extent = pod.extent - anchorOffset;
                if (extent <= 0.0f) {
                    return;
                }
                Vector3 anchor = center;
                if (pod.anchorSet) {
                    Vector3 rotated = Vector3RotateByQuaternion(pod.anchorLocal, transform.rotation);
                    anchor = Vector3Add(transform.position, rotated);
                }
                podDirs[podCount] = dir;
                podExtents[podCount] = extent * progress * (0.85f + 0.15f * strengthScale);
                podAnchors[podCount] = anchor;
                podCount++;
            };

            for (int i = 0; i < components::ECMLocomotion::MaxPods; i++) {
                const auto& pod = locomotion.pods[i];
                if (pod.state == kPodExtend && pod.index >= 0) {
                    float progress = pod.duration > 0.0f ? pod.time / pod.duration : 0.0f;
                    progress = std::clamp(progress, 0.0f, 1.0f);
                    addPod(pod, progress * progress, 1.0f);
                }
            }

            for (int i = 0; i < components::ECMLocomotion::MaxPods; i++) {
                const auto& pod = locomotion.pods[i];
                if (pod.state == kPodHold && pod.index >= 0) {
                    addPod(pod, 1.0f, 0.8f);
                }
            }

            for (int i = 0; i < components::ECMLocomotion::MaxPods; i++) {
                const auto& pod = locomotion.pods[i];
                if (pod.state == 3 && pod.index >= 0) {
                    float progress = pod.duration > 0.0f ? 1.0f - (pod.time / pod.duration) : 0.0f;
                    progress = std::clamp(progress, 0.0f, 1.0f);
                    addPod(pod, progress, 0.6f);
                }
            }

            rendering::setPodData(sdf.shader, uniforms, podDirs, podExtents, podAnchors, podCount);

            constexpr float kPointRadiusScale = 0.65f;
            constexpr float kWarpScale = 0.16f;
            constexpr float kBumpScale = 0.16f;
            constexpr float kJitterMax = 1.06f;
            constexpr float kBasePaddingScale = 0.35f;
            constexpr float kPseudopodPaddingScale = 3.0f;
            float pointRadius = microbe.stats.baseRadius * kPointRadiusScale;
            float padding = pointRadius * (kJitterMax + kWarpScale + kBumpScale) +
                microbe.stats.baseRadius * (kBasePaddingScale + kPseudopodPaddingScale) + 0.05f;

            float sizeX = (maxPos.x - minPos.x) + padding * 2.0f;
            float sizeY = (maxPos.y - minPos.y) + padding * 2.0f;
            float sizeZ = (maxPos.z - minPos.z) + padding * 2.0f;

            BeginShaderMode(sdf.shader);
            DrawCube(center,
                     sizeX,
                     sizeY,
                     sizeZ,
                     WHITE);
            EndShaderMode();
        });
}

} // namespace micro_idle
