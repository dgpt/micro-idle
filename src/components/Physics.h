#ifndef MICRO_IDLE_PHYSICS_H
#define MICRO_IDLE_PHYSICS_H

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyID.h>

namespace components {

struct PhysicsBody {
    JPH::BodyID bodyID;
    float mass{1.0f};
    bool isStatic{false};
};

} // namespace components

#endif
