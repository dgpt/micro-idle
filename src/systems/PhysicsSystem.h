#ifndef MICRO_IDLE_PHYSICS_SYSTEM_H
#define MICRO_IDLE_PHYSICS_SYSTEM_H

#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>

namespace micro_idle {

// Object layers for collision filtering
namespace Layers {
    static constexpr JPH::ObjectLayer NON_MOVING = 0;
    static constexpr JPH::ObjectLayer MOVING = 1;
    static constexpr JPH::ObjectLayer SKIN = 2;        // Soft body skin (collides with ground and skeleton)
    static constexpr JPH::ObjectLayer SKELETON = 3;    // Internal rigid skeleton (collides with skin only, ignores ground)
    static constexpr JPH::uint NUM_LAYERS = 4;
}

// Broad phase layers (for efficient collision detection)
namespace BroadPhaseLayers {
    static constexpr JPH::BroadPhaseLayer NON_MOVING(0);
    static constexpr JPH::BroadPhaseLayer MOVING(1);
    static constexpr JPH::BroadPhaseLayer SKIN(2);
    static constexpr JPH::BroadPhaseLayer SKELETON(3);
    static constexpr JPH::uint NUM_LAYERS(4);
}

// BPLayerInterface maps object layers to broad phase layers
class BPLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface {
public:
    BPLayerInterfaceImpl();
    virtual JPH::uint GetNumBroadPhaseLayers() const override { return BroadPhaseLayers::NUM_LAYERS; }
    virtual JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override;

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    virtual const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override;
#endif // JPH_EXTERNAL_PROFILE || JPH_PROFILE_ENABLED

private:
    JPH::BroadPhaseLayer mObjectToBroadPhase[Layers::NUM_LAYERS];
};

// ObjectVsBroadPhaseLayerFilter - determines if object layer can collide with broad phase layer
class ObjectVsBroadPhaseLayerFilterImpl : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
    virtual bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override;
};

// ObjectLayerPairFilter - determines if two object layers can collide
class ObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter {
public:
    virtual bool ShouldCollide(JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2) const override;
};

// PhysicsSystem singleton - manages Jolt physics world
struct PhysicsSystemState {
    JPH::TempAllocatorImpl* tempAllocator;
    JPH::JobSystemThreadPool* jobSystem;
    BPLayerInterfaceImpl* bpLayerInterface;
    ObjectVsBroadPhaseLayerFilterImpl* objectVsBroadPhaseFilter;
    ObjectLayerPairFilterImpl* objectLayerPairFilter;
    JPH::PhysicsSystem* physicsSystem;

    PhysicsSystemState();
    ~PhysicsSystemState();

    void update(float dt);
    JPH::BodyID createSphere(JPH::Vec3 position, float radius, bool isStatic);
    JPH::BodyID createCylinder(JPH::Vec3 position, float radius, float height, bool isStatic);
    JPH::BodyID createBox(JPH::Vec3 position, JPH::Vec3 halfExtents, bool isStatic);
    void destroyBody(JPH::BodyID bodyID);
};

} // namespace micro_idle

#endif
