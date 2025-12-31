#include "SDFRenderSystem.h"
#include "src/components/Microbe.h"
#include "src/components/Transform.h"
#include "src/components/Rendering.h"
#include "src/rendering/RaymarchBounds.h"
#include "src/rendering/SDFShader.h"
#include "raylib.h"

namespace micro_idle {

void SDFRenderSystem::registerSystem(flecs::world& world) {
    // System that renders microbes using SDF raymarching
    world.system<const components::Microbe, const components::Transform, const components::SDFRenderComponent>("SDFRenderSystem")
        .kind(flecs::PostUpdate)
        .each([&world](const components::Microbe& microbe,
                       const components::Transform& transform,
                       const components::SDFRenderComponent& sdf) {
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

            constexpr float kPointRadiusScale = 0.65f;
            constexpr float kWarpScale = 0.2f;
            constexpr float kBumpScale = 0.22f;
            constexpr float kJitterMax = 1.06f;
            constexpr float kBasePaddingScale = 0.35f;
            constexpr float kPseudopodPaddingScale = 0.9f;
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
