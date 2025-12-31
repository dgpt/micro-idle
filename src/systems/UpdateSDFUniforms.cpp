#include "UpdateSDFUniforms.h"
#include "src/components/Microbe.h"
#include "src/components/Rendering.h"
#include "src/systems/SoftBodyFactory.h"

namespace micro_idle {

void UpdateSDFUniforms::registerSystem(flecs::world& world, PhysicsSystemState* physics) {
    // System that extracts vertex positions from soft bodies and updates shader uniforms
    // Runs in OnStore phase (after TransformSync, before rendering)
    world.system<components::Microbe, components::SDFRenderComponent>("UpdateSDFUniforms")
        .kind(flecs::OnStore)
        .each([physics](components::Microbe& microbe, components::SDFRenderComponent& sdf) {
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
        });
}

} // namespace micro_idle
