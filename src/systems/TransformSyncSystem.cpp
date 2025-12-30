#include "TransformSyncSystem.h"
#include "raylib.h"
#include "src/components/Transform.h"
#include "src/components/Physics.h"
#include "src/components/Microbe.h"
#include "src/systems/PhysicsSystem.h"
#include <Jolt/Physics/Body/BodyInterface.h>

namespace micro_idle {

void TransformSyncSystem::registerSystem(flecs::world& world, PhysicsSystemState* physics) {
    // System that syncs soft body transforms (center of mass) from Jolt to FLECS
    world.system<components::Microbe, components::Transform>("TransformSyncSystem_Soft")
        .kind(flecs::OnStore)
        .each([physics](flecs::entity e, components::Microbe& microbe, components::Transform& transform) {
            if (!microbe.softBody.bodyID.IsInvalid() && physics && physics->physicsSystem) {
                JPH::BodyInterface& bodyInterface = physics->physicsSystem->GetBodyInterface();

                // Get soft body center of mass
                JPH::RVec3 pos = bodyInterface.GetCenterOfMassPosition(microbe.softBody.bodyID);

                // Disabled excessive logging for performance
                // static int callCount = 0;
                // static uint64_t lastBodyID = 0;
                // static JPH::RVec3 lastPos;

                transform.position.x = (float)pos.GetX();
                transform.position.y = (float)pos.GetY();
                transform.position.z = (float)pos.GetZ();

                // Get rotation (for future use)
                JPH::Quat rot = bodyInterface.GetRotation(microbe.softBody.bodyID);
                transform.rotation.x = rot.GetX();
                transform.rotation.y = rot.GetY();
                transform.rotation.z = rot.GetZ();
                transform.rotation.w = rot.GetW();
            }
        });
}

} // namespace micro_idle
