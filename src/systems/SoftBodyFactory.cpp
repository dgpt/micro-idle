#include "SoftBodyFactory.h"
#include <Jolt/Physics/Constraints/DistanceConstraint.h>
#include <cmath>
#include <algorithm>
#include <set>
#include <stdio.h>

namespace micro_idle {

std::vector<Vector3> SoftBodyFactory::generateSpherePoints(int count, float radius) {
    std::vector<Vector3> points;

    // Use golden spiral algorithm for even distribution on sphere surface
    const float goldenRatio = (1.0f + sqrtf(5.0f)) / 2.0f;
    const float angleIncrement = 2.0f * PI * goldenRatio;

    for (int i = 0; i < count; i++) {
        float t = (float)i / (float)count;
        float inclination = acosf(1.0f - 2.0f * t);
        float azimuth = angleIncrement * i;

        float x = sinf(inclination) * cosf(azimuth);
        float y = sinf(inclination) * sinf(azimuth);
        float z = cosf(inclination);

        points.push_back({x * radius, y * radius, z * radius});
    }

    return points;
}

// Generate random points distributed throughout sphere volume (for cytoskeleton)
static std::vector<Vector3> generateVolumePoints(int count, float radius) {
    std::vector<Vector3> points;

    for (int i = 0; i < count; i++) {
        // Random point in sphere using rejection sampling
        float x, y, z, lengthSq;
        do {
            x = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
            y = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
            z = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
            lengthSq = x*x + y*y + z*z;
        } while (lengthSq > 1.0f);

        // Scale to radius
        points.push_back({x * radius, y * radius, z * radius});
    }

    return points;
}

components::SoftBody SoftBodyFactory::createAmoeba(
    PhysicsSystemState* physics,
    Vector3 centerPosition,
    float radius,
    int particleCount
) {
    components::SoftBody softBody;

    // Amoeba structure: Internal skeleton + membrane particle cloud
    softBody.skeletonParticleCount = 6;  // Internal cytoskeleton

    // Generate cytoskeleton particles distributed throughout volume
    std::vector<Vector3> cytoPositions = generateVolumePoints(softBody.skeletonParticleCount, radius * 0.6f);

    // Create Jolt bodies for cytoskeleton particles
    float cytoRadius = radius / 15.0f;
    for (const Vector3& localPos : cytoPositions) {
        Vector3 worldPos = {centerPosition.x + localPos.x, centerPosition.y + localPos.y, centerPosition.z + localPos.z};
        JPH::BodyID bodyID = physics->createSphere(JPH::Vec3(worldPos.x, worldPos.y, worldPos.z), cytoRadius, false);
        softBody.particleBodyIDs.push_back(bodyID);
        softBody.restPositions.push_back(localPos);
    }

    // Generate elastic mesh membrane (icosphere)
    // Mesh vertices will be used for SDF raymarching rendering
    std::vector<Vector3> meshVertices;
    std::vector<int> meshTriangles;
    generateIcosphere(0, radius, meshVertices, meshTriangles);  // 0 subdivisions = 12 vertices (test)

    softBody.membrane.meshVertexStartIndex = (int)softBody.particleBodyIDs.size();
    softBody.membrane.meshVertexCount = (int)meshVertices.size();
    softBody.membrane.triangleIndices = meshTriangles;

    // Create Jolt bodies for mesh vertices
    float meshVertexRadius = radius / 20.0f;
    for (const Vector3& localPos : meshVertices) {
        Vector3 worldPos = {centerPosition.x + localPos.x, centerPosition.y + localPos.y, centerPosition.z + localPos.z};
        JPH::BodyID bodyID = physics->createSphere(JPH::Vec3(worldPos.x, worldPos.y, worldPos.z), meshVertexRadius, false);
        softBody.particleBodyIDs.push_back(bodyID);
        softBody.restPositions.push_back(localPos);
    }

    printf("Creating amoeba: %d skeleton + %d mesh vertices\n",
           softBody.skeletonParticleCount, softBody.membrane.meshVertexCount);
    fflush(stdout);

    // Create constraints
    createSpringConstraints(physics, softBody, 0.9f, 0.3f);

    return softBody;
}

void SoftBodyFactory::createSpringConstraints(
    PhysicsSystemState* physics,
    components::SoftBody& softBody,
    float stiffness,
    float damping
) {
    const JPH::BodyLockInterface& lockInterface = physics->physicsSystem->GetBodyLockInterface();
    int cytoCount = softBody.skeletonParticleCount;
    int meshStart = softBody.membrane.meshVertexStartIndex;
    int meshCount = softBody.membrane.meshVertexCount;

    auto createConstraint = [&](int i, int j, float distance, float minRatio, float maxRatio) {
        JPH::BodyLockWrite lock1(lockInterface, softBody.particleBodyIDs[i]);
        JPH::BodyLockWrite lock2(lockInterface, softBody.particleBodyIDs[j]);
        if (lock1.Succeeded() && lock2.Succeeded()) {
            JPH::DistanceConstraintSettings settings;
            settings.mPoint1 = JPH::RVec3(0, 0, 0);
            settings.mPoint2 = JPH::RVec3(0, 0, 0);
            settings.mMinDistance = distance * minRatio;
            settings.mMaxDistance = distance * maxRatio;
            JPH::TwoBodyConstraint* constraint = static_cast<JPH::TwoBodyConstraint*>(
                settings.Create(lock1.GetBody(), lock2.GetBody())
            );
            physics->physicsSystem->AddConstraint(constraint);
            softBody.constraints.push_back(constraint);
        }
    };

    // 1. Connect mesh edges (from triangle topology) - deduplicate edges
    std::set<std::pair<int, int>> edges;
    for (size_t i = 0; i < softBody.membrane.triangleIndices.size(); i += 3) {
        int v0 = meshStart + softBody.membrane.triangleIndices[i];
        int v1 = meshStart + softBody.membrane.triangleIndices[i + 1];
        int v2 = meshStart + softBody.membrane.triangleIndices[i + 2];

        for (auto [a, b] : {std::pair{v0,v1}, {v1,v2}, {v2,v0}}) {
            if (a > b) std::swap(a, b);  // Normalize edge order
            edges.insert({a, b});
        }
    }

    printf("Creating %zu edge constraints...\n", edges.size());
    for (const auto& [a, b] : edges) {
        Vector3 pa = softBody.restPositions[a];
        Vector3 pb = softBody.restPositions[b];
        float dist = sqrtf((pb.x-pa.x)*(pb.x-pa.x) + (pb.y-pa.y)*(pb.y-pa.y) + (pb.z-pa.z)*(pb.z-pa.z));
        createConstraint(a, b, dist, 0.8f, 1.5f);
    }

    // 2. Anchor mesh vertices to nearest skeleton particles
    for (int m = meshStart; m < meshStart + meshCount; m++) {
        Vector3 meshPos = softBody.restPositions[m];
        std::vector<std::pair<float, int>> distances;
        for (int c = 0; c < cytoCount; c++) {
            Vector3 cytoPos = softBody.restPositions[c];
            float dist = sqrtf((cytoPos.x-meshPos.x)*(cytoPos.x-meshPos.x) +
                              (cytoPos.y-meshPos.y)*(cytoPos.y-meshPos.y) +
                              (cytoPos.z-meshPos.z)*(cytoPos.z-meshPos.z));
            distances.push_back({dist, c});
        }
        std::sort(distances.begin(), distances.end());

        for (int k = 0; k < 2 && k < (int)distances.size(); k++) {
            createConstraint(m, distances[k].second, distances[k].first, 0.5f, 3.0f);
        }
    }

    printf("Created %zu constraints\n", softBody.constraints.size());
}

void SoftBodyFactory::destroySoftBody(
    PhysicsSystemState* physics,
    components::SoftBody& softBody
) {
    // Remove and destroy all constraints
    for (JPH::Constraint* constraint : softBody.constraints) {
        physics->physicsSystem->RemoveConstraint(constraint);
    }
    softBody.constraints.clear();

    // Destroy all particle bodies
    for (JPH::BodyID bodyID : softBody.particleBodyIDs) {
        physics->destroyBody(bodyID);
    }
    softBody.particleBodyIDs.clear();
    softBody.restPositions.clear();
}

// Generate icosphere mesh (subdivided icosahedron)
void SoftBodyFactory::generateIcosphere(int subdivisions, float radius,
                                       std::vector<Vector3>& outVertices,
                                       std::vector<int>& outTriangles) {
    // Start with icosahedron vertices (normalized)
    const float t = (1.0f + sqrtf(5.0f)) / 2.0f;
    const float len = sqrtf(1.0f + t*t);

    outVertices = {
        {-1/len,  t/len,  0/len}, { 1/len,  t/len,  0/len}, {-1/len, -t/len,  0/len}, { 1/len, -t/len,  0/len},
        { 0/len, -1/len,  t/len}, { 0/len,  1/len,  t/len}, { 0/len, -1/len, -t/len}, { 0/len,  1/len, -t/len},
        { t/len,  0/len, -1/len}, { t/len,  0/len,  1/len}, {-t/len,  0/len, -1/len}, {-t/len,  0/len,  1/len}
    };

    outTriangles = {
        0,11,5,  0,5,1,  0,1,7,  0,7,10, 0,10,11,
        1,5,9,   5,11,4, 11,10,2, 10,7,6, 7,1,8,
        3,9,4,   3,4,2,  3,2,6,   3,6,8,  3,8,9,
        4,9,5,   2,4,11, 6,2,10,  8,6,7,  9,8,1
    };

    // Subdivide to make smoother
    for (int sub = 0; sub < subdivisions; sub++) {
        std::vector<int> newTriangles;
        for (size_t i = 0; i < outTriangles.size(); i += 3) {
            int v0 = outTriangles[i];
            int v1 = outTriangles[i + 1];
            int v2 = outTriangles[i + 2];

            // Calculate midpoints
            Vector3 m01 = {
                (outVertices[v0].x + outVertices[v1].x) / 2.0f,
                (outVertices[v0].y + outVertices[v1].y) / 2.0f,
                (outVertices[v0].z + outVertices[v1].z) / 2.0f
            };
            Vector3 m12 = {
                (outVertices[v1].x + outVertices[v2].x) / 2.0f,
                (outVertices[v1].y + outVertices[v2].y) / 2.0f,
                (outVertices[v1].z + outVertices[v2].z) / 2.0f
            };
            Vector3 m20 = {
                (outVertices[v2].x + outVertices[v0].x) / 2.0f,
                (outVertices[v2].y + outVertices[v0].y) / 2.0f,
                (outVertices[v2].z + outVertices[v0].z) / 2.0f
            };

            // Normalize to unit sphere
            float len01 = sqrtf(m01.x*m01.x + m01.y*m01.y + m01.z*m01.z);
            float len12 = sqrtf(m12.x*m12.x + m12.y*m12.y + m12.z*m12.z);
            float len20 = sqrtf(m20.x*m20.x + m20.y*m20.y + m20.z*m20.z);
            m01.x /= len01; m01.y /= len01; m01.z /= len01;
            m12.x /= len12; m12.y /= len12; m12.z /= len12;
            m20.x /= len20; m20.y /= len20; m20.z /= len20;

            int i01 = (int)outVertices.size();
            int i12 = i01 + 1;
            int i20 = i01 + 2;
            outVertices.push_back(m01);
            outVertices.push_back(m12);
            outVertices.push_back(m20);

            // 4 new triangles
            newTriangles.insert(newTriangles.end(), {v0, i01, i20});
            newTriangles.insert(newTriangles.end(), {v1, i12, i01});
            newTriangles.insert(newTriangles.end(), {v2, i20, i12});
            newTriangles.insert(newTriangles.end(), {i01, i12, i20});
        }
        outTriangles = newTriangles;
    }

    // Scale to radius
    for (auto& v : outVertices) {
        v.x *= radius;
        v.y *= radius;
        v.z *= radius;
    }
}

} // namespace micro_idle
