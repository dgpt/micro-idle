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
    // Runs in PostUpdate phase (final phase, after all simulation)
    world.system<const components::Microbe, const components::Transform, const components::SDFRenderComponent>("SDFRenderSystem")
        .kind(flecs::PostUpdate)
        .each([](flecs::entity e, const components::Microbe& microbe, const components::Transform& transform, const components::SDFRenderComponent& sdf) {
            if (sdf.vertexCount == 0 || sdf.shader.id == 0 || !sdf.shaderLoaded) {
                return; // Skip if no vertices or shader not loaded
            }

            // Calculate bounding sphere for the microbe using utility
            Vector3 center = transform.position;
            float boundRadius = rendering::calculateBoundRadius(microbe.stats.baseRadius);

            // Begin shader mode
            BeginShaderMode(sdf.shader);

            // Set shader uniforms using utility functions
            // Note: Vertex positions are already set by UpdateSDFUniforms
            // Camera position is set globally before rendering (see World::render)

            // Build uniform struct from component (for compatibility)
            rendering::SDFShaderUniforms uniforms;
            uniforms.viewPos = sdf.shaderLocViewPos;
            uniforms.pointCount = sdf.shaderLocPointCount;
            uniforms.baseRadius = sdf.shaderLocBaseRadius;
            uniforms.microbeColor = sdf.shaderLocMicrobeColor;
            for (int i = 0; i < 64; i++) {
                uniforms.skeletonPoints[i] = sdf.shaderLocSkeletonPoints[i];
            }

            rendering::setMicrobeUniforms(sdf.shader, uniforms, sdf.vertexCount,
                                         microbe.stats.baseRadius, microbe.stats.color);

            // Draw bounding sphere (the shader will raymarch inside it)
            DrawSphere(center, boundRadius, WHITE);

            EndShaderMode();
        });
}

} // namespace micro_idle
