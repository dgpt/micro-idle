#include "PhysicsSystem.h"
#include <stdio.h>
#include <cstdarg>

// Jolt uses callbacks for trace and asserts
static void TraceImpl(const char* inFMT, ...) {
    va_list list;
    va_start(list, inFMT);
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), inFMT, list);
    va_end(list);
    printf("Jolt: %s\n", buffer);
}

#ifdef JPH_ENABLE_ASSERTS
static bool AssertFailedImpl(const char* inExpression, const char* inMessage, const char* inFile, JPH::uint inLine) {
    printf("Jolt Assert Failed: %s:%u: (%s) %s\n", inFile, inLine, inExpression, inMessage ? inMessage : "");
    return true; // Trigger breakpoint
}
#endif

namespace micro_idle {

// BPLayerInterface implementation
BPLayerInterfaceImpl::BPLayerInterfaceImpl() {
    mObjectToBroadPhase[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
    mObjectToBroadPhase[Layers::MOVING] = BroadPhaseLayers::MOVING;
}

JPH::BroadPhaseLayer BPLayerInterfaceImpl::GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const {
    JPH_ASSERT(inLayer < Layers::NUM_LAYERS);
    return mObjectToBroadPhase[inLayer];
}


// ObjectVsBroadPhaseLayerFilter implementation
bool ObjectVsBroadPhaseLayerFilterImpl::ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const {
    switch (inLayer1) {
        case Layers::NON_MOVING:
            return inLayer2 == BroadPhaseLayers::MOVING;
        case Layers::MOVING:
            return true; // Moving objects collide with everything
        default:
            JPH_ASSERT(false);
            return false;
    }
}

// ObjectLayerPairFilter implementation
bool ObjectLayerPairFilterImpl::ShouldCollide(JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2) const {
    switch (inObject1) {
        case Layers::NON_MOVING:
            return inObject2 == Layers::MOVING; // Non-moving only collides with moving
        case Layers::MOVING:
            return true; // Moving collides with everything
        default:
            JPH_ASSERT(false);
            return false;
    }
}

// PhysicsSystemState implementation
PhysicsSystemState::PhysicsSystemState() {
    printf("PhysicsSystem: Initializing Jolt Physics...\n");
    fflush(stdout);

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

    // No gravity for 2D simulation on XZ plane
    physicsSystem->SetGravity(JPH::Vec3(0, 0, 0));

    printf("PhysicsSystem: Jolt initialized (max bodies: %u, threads: %u)\n", cMaxBodies, std::thread::hardware_concurrency());
    fflush(stdout);
}

PhysicsSystemState::~PhysicsSystemState() {
    printf("PhysicsSystem: Shutting down Jolt Physics\n");
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
        printf("PhysicsSystem: Failed to create sphere body\n");
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
        printf("PhysicsSystem: Failed to create cylinder body\n");
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
        printf("PhysicsSystem: Failed to create box body\n");
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
