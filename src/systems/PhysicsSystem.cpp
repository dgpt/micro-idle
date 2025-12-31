#include "PhysicsSystem.h"

// Jolt uses callbacks for trace and asserts
static void TraceImpl(const char* inFMT, ...) {
    (void)inFMT;
}

#ifdef JPH_ENABLE_ASSERTS
static bool AssertFailedImpl(const char* inExpression, const char* inMessage, const char* inFile, JPH::uint inLine) {
    (void)inExpression;
    (void)inMessage;
    (void)inFile;
    (void)inLine;
    return true; // Trigger breakpoint
}
#endif

namespace micro_idle {

// BPLayerInterface implementation
BPLayerInterfaceImpl::BPLayerInterfaceImpl() {
    mObjectToBroadPhase[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
    mObjectToBroadPhase[Layers::MOVING] = BroadPhaseLayers::MOVING;
    mObjectToBroadPhase[Layers::SKIN] = BroadPhaseLayers::SKIN;
    mObjectToBroadPhase[Layers::SKELETON] = BroadPhaseLayers::SKELETON;
}

JPH::BroadPhaseLayer BPLayerInterfaceImpl::GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const {
    JPH_ASSERT(inLayer < Layers::NUM_LAYERS);
    return mObjectToBroadPhase[inLayer];
}

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
const char* BPLayerInterfaceImpl::GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const {
    switch ((JPH::BroadPhaseLayer::Type)inLayer) {
        case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::NON_MOVING: return "NON_MOVING";
        case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::MOVING: return "MOVING";
        case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::SKIN: return "SKIN";
        case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::SKELETON: return "SKELETON";
        default: JPH_ASSERT(false); return "UNKNOWN";
    }
}
#endif // JPH_EXTERNAL_PROFILE || JPH_PROFILE_ENABLED


// ObjectVsBroadPhaseLayerFilter implementation
bool ObjectVsBroadPhaseLayerFilterImpl::ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const {
    switch (inLayer1) {
        case Layers::NON_MOVING:
            return inLayer2 == BroadPhaseLayers::MOVING || inLayer2 == BroadPhaseLayers::SKIN;
        case Layers::MOVING:
            return true; // Moving objects collide with everything
        case Layers::SKIN:
            return inLayer2 == BroadPhaseLayers::NON_MOVING || inLayer2 == BroadPhaseLayers::SKELETON ||
                   inLayer2 == BroadPhaseLayers::SKIN;
        case Layers::SKELETON:
            return inLayer2 == BroadPhaseLayers::SKIN; // Skeleton only collides with skin, ignores ground
        default:
            JPH_ASSERT(false);
            return false;
    }
}

// ObjectLayerPairFilter implementation
bool ObjectLayerPairFilterImpl::ShouldCollide(JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2) const {
    switch (inObject1) {
        case Layers::NON_MOVING:
            return inObject2 == Layers::MOVING || inObject2 == Layers::SKIN;
        case Layers::MOVING:
            return true; // Moving collides with everything
        case Layers::SKIN:
            return inObject2 == Layers::NON_MOVING || inObject2 == Layers::SKELETON || inObject2 == Layers::SKIN; // SKIN collides with SKIN for microbe-microbe collisions
        case Layers::SKELETON:
            return inObject2 == Layers::SKIN; // Skeleton only collides with skin, ignores ground
        default:
            JPH_ASSERT(false);
            return false;
    }
}

// PhysicsSystemState implementation
PhysicsSystemState::PhysicsSystemState() {
    // Register allocation hook
    JPH::RegisterDefaultAllocator();

    // Install callbacks
    JPH::Trace = TraceImpl;
#ifdef JPH_ENABLE_ASSERTS
    JPH::AssertFailed = AssertFailedImpl;
#endif

    // Register all Jolt physics types
    JPH::Factory::sInstance = new JPH::Factory();
    JPH::RegisterTypes();

    // Create temp allocator (need enough for long simulations)
    tempAllocator = new JPH::TempAllocatorImpl(100 * 1024 * 1024); // 100 MB

    // Create job system (use all cores)
    jobSystem = new JPH::JobSystemThreadPool(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, std::thread::hardware_concurrency() - 1);

    // Create layer interfaces
    bpLayerInterface = new BPLayerInterfaceImpl();
    objectVsBroadPhaseFilter = new ObjectVsBroadPhaseLayerFilterImpl();
    objectLayerPairFilter = new ObjectLayerPairFilterImpl();

    // Create physics system
    const JPH::uint cMaxBodies = 10240;
    const JPH::uint cNumBodyMutexes = 0; // Auto-detect
    const JPH::uint cMaxBodyPairs = 65536;
    const JPH::uint cMaxContactConstraints = 20480;

    physicsSystem = new JPH::PhysicsSystem();
    physicsSystem->Init(
        cMaxBodies,
        cNumBodyMutexes,
        cMaxBodyPairs,
        cMaxContactConstraints,
        *bpLayerInterface,
        *objectVsBroadPhaseFilter,
        *objectLayerPairFilter
    );

    // Enable gravity so microbes fall back to petri dish (Y=0) if they get picked up or spawned high
    physicsSystem->SetGravity(JPH::Vec3(0, -9.81f, 0));

}

PhysicsSystemState::~PhysicsSystemState() {
    delete physicsSystem;
    delete objectLayerPairFilter;
    delete objectVsBroadPhaseFilter;
    delete bpLayerInterface;
    delete jobSystem;
    delete tempAllocator;
    delete JPH::Factory::sInstance;
    JPH::Factory::sInstance = nullptr;
}

void PhysicsSystemState::update(float dt) {
    // Step the physics simulation
    const int cCollisionSteps = 1;
    physicsSystem->Update(dt, cCollisionSteps, tempAllocator, jobSystem);
}

JPH::BodyID PhysicsSystemState::createSphere(JPH::Vec3 position, float radius, bool isStatic) {
    JPH::BodyInterface& bodyInterface = physicsSystem->GetBodyInterface();

    // Create sphere shape
    JPH::SphereShapeSettings sphereSettings(radius);
    JPH::ShapeSettings::ShapeResult shapeResult = sphereSettings.Create();
    JPH::ShapeRefC sphereShape = shapeResult.Get();

    // Create body settings
    JPH::BodyCreationSettings bodySettings(
        sphereShape,
        JPH::RVec3(position.GetX(), position.GetY(), position.GetZ()),
        JPH::Quat::sIdentity(),
        isStatic ? JPH::EMotionType::Static : JPH::EMotionType::Dynamic,
        isStatic ? Layers::NON_MOVING : Layers::MOVING
    );

    // Lock Y axis for dynamic bodies (2D simulation on XZ plane)
    if (!isStatic) {
        bodySettings.mAllowedDOFs = JPH::EAllowedDOFs::TranslationX | JPH::EAllowedDOFs::TranslationZ;
    }

    // Create body
    JPH::Body* body = bodyInterface.CreateBody(bodySettings);
    if (!body) {
        return JPH::BodyID();
    }

    JPH::BodyID bodyID = body->GetID();
    bodyInterface.AddBody(bodyID, JPH::EActivation::Activate);

    return bodyID;
}

JPH::BodyID PhysicsSystemState::createCylinder(JPH::Vec3 position, float radius, float height, bool isStatic) {
    JPH::BodyInterface& bodyInterface = physicsSystem->GetBodyInterface();

    // Create cylinder shape (halfHeight is used by Jolt)
    JPH::CylinderShapeSettings cylinderSettings(height / 2.0f, radius);
    JPH::ShapeSettings::ShapeResult shapeResult = cylinderSettings.Create();
    JPH::ShapeRefC cylinderShape = shapeResult.Get();

    // Create body settings
    JPH::BodyCreationSettings bodySettings(
        cylinderShape,
        JPH::RVec3(position.GetX(), position.GetY(), position.GetZ()),
        JPH::Quat::sIdentity(),
        isStatic ? JPH::EMotionType::Static : JPH::EMotionType::Dynamic,
        isStatic ? Layers::NON_MOVING : Layers::MOVING
    );

    // Create body
    JPH::Body* body = bodyInterface.CreateBody(bodySettings);
    if (!body) {
        return JPH::BodyID();
    }

    JPH::BodyID bodyID = body->GetID();
    bodyInterface.AddBody(bodyID, JPH::EActivation::Activate);

    return bodyID;
}

JPH::BodyID PhysicsSystemState::createBox(JPH::Vec3 position, JPH::Vec3 halfExtents, bool isStatic) {
    JPH::BodyInterface& bodyInterface = physicsSystem->GetBodyInterface();

    // Create box shape
    JPH::BoxShapeSettings boxSettings(halfExtents);
    JPH::ShapeSettings::ShapeResult shapeResult = boxSettings.Create();
    JPH::ShapeRefC boxShape = shapeResult.Get();

    // Create body settings
    JPH::BodyCreationSettings bodySettings(
        boxShape,
        JPH::RVec3(position.GetX(), position.GetY(), position.GetZ()),
        JPH::Quat::sIdentity(),
        isStatic ? JPH::EMotionType::Static : JPH::EMotionType::Dynamic,
        isStatic ? Layers::NON_MOVING : Layers::MOVING
    );

    // Create body
    JPH::Body* body = bodyInterface.CreateBody(bodySettings);
    if (!body) {
        return JPH::BodyID();
    }

    JPH::BodyID bodyID = body->GetID();
    bodyInterface.AddBody(bodyID, JPH::EActivation::Activate);

    return bodyID;
}

void PhysicsSystemState::destroyBody(JPH::BodyID bodyID) {
    if (!bodyID.IsInvalid()) {
        JPH::BodyInterface& bodyInterface = physicsSystem->GetBodyInterface();
        bodyInterface.RemoveBody(bodyID);
        bodyInterface.DestroyBody(bodyID);
    }
}

} // namespace micro_idle
