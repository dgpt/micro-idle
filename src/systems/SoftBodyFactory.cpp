#include "SoftBodyFactory.h"
#include "PhysicsSystem.h"
#include "src/physics/Icosphere.h"
#include "src/physics/Constraints.h"

#include <Jolt/Physics/SoftBody/SoftBodyCreationSettings.h>
#include <Jolt/Physics/SoftBody/SoftBodyMotionProperties.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Constraints/DistanceConstraint.h>
#include <stdio.h>

namespace micro_idle {

JPH::BodyID SoftBodyFactory::CreateAmoeba(
    PhysicsSystemState* physics,
    Vector3 position,
    float radius,
    int subdivisions
) {
    printf("SoftBodyFactory: Creating amoeba (subdivisions=%d, radius=%.2f)\n", subdivisions, radius);

    // Step 1: Generate icosphere mesh
    IcosphereMesh mesh = GenerateIcosphere(subdivisions, radius);
    printf("  Generated icosphere: %d vertices, %d triangles\n",
           mesh.vertexCount, mesh.triangleCount);

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
    vertexAttribs.mCompliance = ConstraintPresets::Amoeba.compliance;
    vertexAttribs.mShearCompliance = ConstraintPresets::Amoeba.compliance * 2.0f;  // Softer shear
    vertexAttribs.mBendCompliance = ConstraintPresets::Amoeba.compliance * 3.0f;   // Softer bend

    sharedSettings->CreateConstraints(&vertexAttribs, 1,
                                       JPH::SoftBodySharedSettings::EBendType::Distance);

    // Optimize the soft body for parallel execution
    sharedSettings->Optimize();

    printf("  Created constraints and optimized\n");

    // Step 4: Create SoftBodyCreationSettings
    JPH::SoftBodyCreationSettings creationSettings(
        sharedSettings,
        JPH::RVec3(position.x, position.y, position.z),
        JPH::Quat::sIdentity(),
        Layers::MOVING
    );

    // Configure soft body physics properties
    creationSettings.mPressure = 20.0f;            // Lower pressure allows more deformation
    creationSettings.mRestitution = 0.1f;          // Minimal bounciness - amoebas don't bounce
    creationSettings.mFriction = 8.0f;             // VERY high friction for substrate grip (0-10 range)
    creationSettings.mLinearDamping = 0.4f;        // Higher damping to prevent excessive motion
    creationSettings.mGravityFactor = 2.0f;        // Strong gravity - keeps them on ground
    creationSettings.mNumIterations = 16;          // Higher for stability with soft constraints
    creationSettings.mUpdatePosition = true;       // Update body position
    creationSettings.mMakeRotationIdentity = true; // Bake rotation into vertices
    creationSettings.mAllowSleeping = false;       // Keep always active for gameplay

    // Step 5: Create and add the soft body
    JPH::BodyID bodyID = physics->physicsSystem->GetBodyInterface().CreateAndAddSoftBody(
        creationSettings,
        JPH::EActivation::Activate
    );

    if (bodyID.IsInvalid()) {
        printf("  ERROR: Failed to create soft body\n");
        delete sharedSettings;
        return JPH::BodyID();
    }

    printf("  Soft body created successfully (BodyID: %u)\n", bodyID.GetIndexAndSequenceNumber());

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

    // Get the center of mass transform to convert from local to world space
    JPH::RVec3 comPos = body.GetCenterOfMassPosition();
    JPH::Quat comRot = body.GetRotation();
    JPH::RMat44 comTransform = JPH::RMat44::sRotationTranslation(comRot, comPos);

    for (int i = 0; i < count; i++) {
        // Transform from local space (relative to center of mass) to world space
        JPH::Vec3 localPos = vertices[i].mPosition;
        JPH::Vec3 worldPos = comTransform * localPos;
        outPositions[i].x = worldPos.GetX();
        outPositions[i].y = worldPos.GetY();
        outPositions[i].z = worldPos.GetZ();
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
