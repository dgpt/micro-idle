#include "SoftBodyFactory.h"
#include "PhysicsSystem.h"
#include "src/physics/Icosphere.h"
#include "src/physics/Constraints.h"

#include <Jolt/Physics/SoftBody/SoftBodyCreationSettings.h>
#include <Jolt/Physics/SoftBody/SoftBodyMotionProperties.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Constraints/DistanceConstraint.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <vector>
#include <cmath>

namespace micro_idle {

JPH::BodyID SoftBodyFactory::CreateAmoeba(
    PhysicsSystemState* physics,
    Vector3 position,
    float radius,
    int subdivisions,
    std::vector<JPH::BodyID>& outSkeletonBodyIDs
) {

    // Step 1: Generate icosphere mesh
    IcosphereMesh mesh = GenerateIcosphere(subdivisions, radius);
    float flatten = 0.25f;
    for (int i = 0; i < mesh.vertexCount; i++) {
        mesh.vertices[i].y *= flatten;
    }

    // Step 2: Create SoftBodySharedSettings
    auto sharedSettings = new JPH::SoftBodySharedSettings();

    // Add vertices
    for (int i = 0; i < mesh.vertexCount; i++) {
        Vector3 v = mesh.vertices[i];
        JPH::SoftBodySharedSettings::Vertex vertex;
        vertex.mPosition = JPH::Float3(v.x, v.y, v.z);
        vertex.mVelocity = JPH::Float3(0, 0, 0);
        vertex.mInvMass = 1.0f;  // All vertices have equal mass
        sharedSettings->mVertices.push_back(vertex);
    }

    // Add faces (triangles)
    for (size_t i = 0; i < mesh.triangles.size(); i += 3) {
        JPH::SoftBodySharedSettings::Face face;
        face.mVertex[0] = mesh.triangles[i];
        face.mVertex[1] = mesh.triangles[i + 1];
        face.mVertex[2] = mesh.triangles[i + 2];
        face.mMaterialIndex = 0;  // Default material
        sharedSettings->mFaces.push_back(face);
    }

    // Step 3: Create constraints automatically
    // Use Amoeba preset for soft, deformable behavior
    JPH::SoftBodySharedSettings::VertexAttributes vertexAttribs;
    float compliance = ConstraintPresets::Amoeba.compliance * 2.2f;
    vertexAttribs.mCompliance = compliance;
    vertexAttribs.mShearCompliance = compliance * 1.2f;  // Allow shear for stretch
    vertexAttribs.mBendCompliance = compliance * 1.3f;   // Resist folding tears
    vertexAttribs.mLRAType = JPH::SoftBodySharedSettings::ELRAType::EuclideanDistance;
    vertexAttribs.mLRAMaxDistanceMultiplier = 1.7f;

    sharedSettings->CreateConstraints(&vertexAttribs, 1,
                                       JPH::SoftBodySharedSettings::EBendType::Distance);

    // Optimize the soft body for parallel execution
    sharedSettings->Optimize();


    // Step 4: Create SoftBodyCreationSettings
    JPH::SoftBodyCreationSettings creationSettings(
        sharedSettings,
        JPH::RVec3(position.x, position.y, position.z),
        JPH::Quat::sIdentity(),
        Layers::SKIN  // Skin layer: collides with ground and skeleton
    );

    // Configure soft body physics properties for a thick, gel-like response
    creationSettings.mPressure = 0.4f;             // Lower pressure for more deformable membrane
    creationSettings.mRestitution = 0.0f;          // No bounce for gel-like response
    creationSettings.mFriction = 1.8f;             // Grip for crawling without locking motion
    creationSettings.mLinearDamping = 2.4f;        // Damping for gel-like response
    creationSettings.mGravityFactor = 2.2f;        // Heavier to keep contact with substrate
    creationSettings.mNumIterations = 24;          // Stability for stiffer constraints
    creationSettings.mMaxLinearVelocity = std::max(3.0f, radius * 9.0f);
    creationSettings.mUpdatePosition = true;       // Update body position
    creationSettings.mMakeRotationIdentity = true; // Bake rotation into vertices
    creationSettings.mAllowSleeping = false;       // Keep always active for gameplay

    // Step 5: Create and add the soft body
    JPH::BodyID bodyID = physics->physicsSystem->GetBodyInterface().CreateAndAddSoftBody(
        creationSettings,
        JPH::EActivation::Activate
    );

    if (bodyID.IsInvalid()) {
        delete sharedSettings;
        return JPH::BodyID();
    }


    // Step 6: Create internal rigid skeleton (Internal Motor model)
    // Skeleton nodes are rigid spheres inside the soft body that push against the skin
    // Skeleton collides with skin but ignores ground (friction-based locomotion)
    JPH::BodyInterface& bodyInterface = physics->physicsSystem->GetBodyInterface();

    // Internal skeleton disabled: EC&M forces now drive soft-body vertices directly
    int skeletonNodeCount = 0;
    float skeletonRadius = radius * 0.15f;  // Skeleton nodes are smaller than soft body
    float skeletonSpacing = radius * 0.4f;   // Spacing between skeleton nodes

    outSkeletonBodyIDs.clear();
    outSkeletonBodyIDs.reserve(skeletonNodeCount);

    for (int i = 0; i < skeletonNodeCount; i++) {
        // Position skeleton nodes in a line along X axis (can be customized)
        float offsetX = (i - (skeletonNodeCount - 1) * 0.5f) * skeletonSpacing;
        JPH::Vec3 skeletonPos(
            position.x + offsetX,
            position.y,  // Same Y as soft body center
            position.z
        );

        // Create sphere shape for skeleton node
        JPH::SphereShapeSettings sphereSettings(skeletonRadius);
        JPH::ShapeSettings::ShapeResult shapeResult = sphereSettings.Create();
        JPH::ShapeRefC sphereShape = shapeResult.Get();

        // Create rigid body for skeleton node
        JPH::BodyCreationSettings skeletonSettings(
            sphereShape,
            JPH::RVec3(skeletonPos.GetX(), skeletonPos.GetY(), skeletonPos.GetZ()),
            JPH::Quat::sIdentity(),
            JPH::EMotionType::Dynamic,
            Layers::SKELETON  // Skeleton layer: collides with skin only, ignores ground
        );

        // Lock Y axis for 2D simulation
        skeletonSettings.mAllowedDOFs = JPH::EAllowedDOFs::TranslationX | JPH::EAllowedDOFs::TranslationZ;
        skeletonSettings.mLinearDamping = 0.8f;
        skeletonSettings.mAngularDamping = 0.8f;

        // Create and add skeleton body
        JPH::Body* skeletonBody = bodyInterface.CreateBody(skeletonSettings);
        if (skeletonBody) {
            JPH::BodyID skeletonBodyID = skeletonBody->GetID();
            bodyInterface.AddBody(skeletonBodyID, JPH::EActivation::Activate);
            outSkeletonBodyIDs.push_back(skeletonBodyID);
        }
    }


    return bodyID;
}

int SoftBodyFactory::ExtractVertexPositions(
    PhysicsSystemState* physics,
    JPH::BodyID bodyID,
    Vector3* outPositions,
    int maxPositions
) {
    if (bodyID.IsInvalid()) {
        return 0;
    }

    JPH::BodyLockRead lock(physics->physicsSystem->GetBodyLockInterface(), bodyID);
    if (!lock.Succeeded()) {
        return 0;
    }

    const JPH::Body& body = lock.GetBody();
    const JPH::SoftBodyMotionProperties* motionProps =
        static_cast<const JPH::SoftBodyMotionProperties*>(body.GetMotionProperties());

    if (motionProps == nullptr) {
        return 0;
    }

    // Get vertex positions from soft body
    const JPH::Array<JPH::SoftBodyVertex>& vertices = motionProps->GetVertices();
    uint32_t vertexCount = (uint32_t)vertices.size();

    int count = (int)vertexCount < maxPositions ? (int)vertexCount : maxPositions;
    if (count <= 0) {
        return 0;
    }

    // Get the center of mass transform to convert from local to world space
    JPH::RVec3 comPos = body.GetCenterOfMassPosition();
    JPH::Quat comRot = body.GetRotation();
    JPH::RMat44 comTransform = JPH::RMat44::sRotationTranslation(comRot, comPos);

    if ((int)vertexCount <= maxPositions) {
        for (int i = 0; i < count; i++) {
            // Transform from local space (relative to center of mass) to world space
            JPH::Vec3 localPos = vertices[i].mPosition;
            JPH::Vec3 worldPos = comTransform * localPos;
            outPositions[i].x = worldPos.GetX();
            outPositions[i].y = worldPos.GetY();
            outPositions[i].z = worldPos.GetZ();
        }
    } else if (count == 1) {
        JPH::Vec3 localPos = vertices[0].mPosition;
        JPH::Vec3 worldPos = comTransform * localPos;
        outPositions[0].x = worldPos.GetX();
        outPositions[0].y = worldPos.GetY();
        outPositions[0].z = worldPos.GetZ();
    } else {
        float step = (float)(vertexCount - 1) / (float)(count - 1);
        for (int i = 0; i < count; i++) {
            int idx = (int)(i * step + 0.5f);
            JPH::Vec3 localPos = vertices[idx].mPosition;
            JPH::Vec3 worldPos = comTransform * localPos;
            outPositions[i].x = worldPos.GetX();
            outPositions[i].y = worldPos.GetY();
            outPositions[i].z = worldPos.GetZ();
        }
    }

    return count;
}

int SoftBodyFactory::GetVertexCount(
    PhysicsSystemState* physics,
    JPH::BodyID bodyID
) {
    if (bodyID.IsInvalid()) {
        return 0;
    }

    JPH::BodyLockRead lock(physics->physicsSystem->GetBodyLockInterface(), bodyID);
    if (!lock.Succeeded()) {
        return 0;
    }

    const JPH::Body& body = lock.GetBody();
    const JPH::SoftBodyMotionProperties* motionProps =
        static_cast<const JPH::SoftBodyMotionProperties*>(body.GetMotionProperties());

    if (motionProps == nullptr) {
        return 0;
    }

    return (int)motionProps->GetVertices().size();
}

} // namespace micro_idle
