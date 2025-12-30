#include "UpdateSDFUniforms.h"
#include "src/components/Microbe.h"
#include "src/components/Rendering.h"
#include "src/systems/SoftBodyFactory.h"
#include "src/rendering/SDFShader.h"
#include "raylib.h"

namespace micro_idle {

void UpdateSDFUniforms::registerSystem(flecs::world& world, PhysicsSystemState* physics) {
    // System that extracts vertex positions from soft bodies and updates shader uniforms
    // Runs in OnStore phase (after TransformSync, before rendering)
    world.system<components::Microbe, components::SDFRenderComponent>("UpdateSDFUniforms")
        .kind(flecs::OnStore)
        .each([physics](flecs::entity e, components::Microbe& microbe, components::SDFRenderComponent& sdf) {
            if (microbe.softBody.vertexCount == 0 || microbe.softBody.bodyID.IsInvalid()) {
                sdf.vertexCount = 0;
                return;
            }

            // Extract vertex positions from Jolt soft body
            int count = SoftBodyFactory::ExtractVertexPositions(
                physics,
                microbe.softBody.bodyID,
                sdf.vertexPositions,
                64
            );

            sdf.vertexCount = count;

            // Cache uniform locations on first run (if shader is loaded)
            if (!sdf.shaderLoaded && sdf.shader.id != 0) {
                rendering::SDFShaderUniforms uniforms;
                if (rendering::initializeSDFUniforms(sdf.shader, uniforms)) {
                    // Copy uniform locations to component
                    sdf.shaderLocViewPos = uniforms.viewPos;
                    sdf.shaderLocPointCount = uniforms.pointCount;
                    sdf.shaderLocBaseRadius = uniforms.baseRadius;
                    sdf.shaderLocMicrobeColor = uniforms.microbeColor;
                    for (int i = 0; i < 64; i++) {
                        sdf.shaderLocSkeletonPoints[i] = uniforms.skeletonPoints[i];
                    }
                    sdf.shaderLoaded = true;
                }
            }

            // Don't set shader uniforms here - will be done during rendering
            // Just cache the vertex positions in the component
            // The rendering loop will set uniforms and draw immediately
        });
}

} // namespace micro_idle
