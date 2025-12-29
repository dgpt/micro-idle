#include "src/physics/Constraints.h"
#include "src/physics/Icosphere.h"
#include <stdio.h>
#include <cmath>

using namespace micro_idle;

static int expect_equal_i(const char* label, int a, int expected) {
    if (a != expected) {
        printf("constraints %s: expected %d, got %d\n", label, expected, a);
        return 1;
    }
    return 0;
}

static int expect_near_f(const char* label, float a, float expected, float epsilon) {
    float diff = fabsf(a - expected);
    if (diff > epsilon) {
        printf("constraints %s: expected %.4f, got %.4f (diff %.6f)\n", label, expected, a, diff);
        return 1;
    }
    return 0;
}

int test_constraints_run(void) {
    int fails = 0;

    // Test edge constraint generation for subdivision 0
    {
        IcosphereMesh mesh = GenerateIcosphere(0, 1.0f);
        auto edges = GenerateEdgeConstraints(mesh, ConstraintPresets::Amoeba);

        // Base icosahedron has 30 edges
        fails += expect_equal_i("edge_count_subdiv0", (int)edges.size(), 30);

        // Verify all edge indices are valid
        for (const auto& edge : edges) {
            if (edge.mVertex[0] >= (JPH::uint32)mesh.vertexCount ||
                edge.mVertex[1] >= (JPH::uint32)mesh.vertexCount) {
                printf("constraints: invalid edge vertex index (%u, %u), vertexCount=%d\n",
                       edge.mVertex[0], edge.mVertex[1], mesh.vertexCount);
                fails++;
            }

            // Verify vertices are different
            if (edge.mVertex[0] == edge.mVertex[1]) {
                printf("constraints: edge connects vertex to itself (%u)\n", edge.mVertex[0]);
                fails++;
            }
        }

        // Verify compliance matches preset
        for (const auto& edge : edges) {
            fails += expect_near_f("edge_compliance_amoeba", edge.mCompliance,
                                   ConstraintPresets::Amoeba.compliance, 0.0001f);
        }
    }

    // Test edge constraint generation for subdivision 1
    {
        IcosphereMesh mesh = GenerateIcosphere(1, 1.0f);
        auto edges = GenerateEdgeConstraints(mesh, ConstraintPresets::SoftSphere);

        // Subdivision 1 has 120 edges
        fails += expect_equal_i("edge_count_subdiv1", (int)edges.size(), 120);

        // Verify compliance matches preset
        if (edges.size() > 0) {
            fails += expect_near_f("edge_compliance_soft", edges[0].mCompliance,
                                   ConstraintPresets::SoftSphere.compliance, 0.0001f);
        }
    }

    // Test different constraint presets
    {
        IcosphereMesh mesh = GenerateIcosphere(0, 1.0f);

        // Rigid sphere preset
        auto rigidEdges = GenerateEdgeConstraints(mesh, ConstraintPresets::RigidSphere);
        if (rigidEdges.size() > 0) {
            fails += expect_near_f("rigid_compliance", rigidEdges[0].mCompliance, 0.0f, 0.0001f);
        }

        // Jelly sphere preset
        auto jellyEdges = GenerateEdgeConstraints(mesh, ConstraintPresets::JellySphere);
        if (jellyEdges.size() > 0) {
            fails += expect_near_f("jelly_compliance", jellyEdges[0].mCompliance, 0.01f, 0.0001f);
        }
    }

    // Test volume constraint generation
    {
        IcosphereMesh mesh = GenerateIcosphere(0, 1.0f);

        // Test different pressure coefficients
        auto volumeLow = GenerateVolumeConstraint(mesh, 0.3f);
        auto volumeMid = GenerateVolumeConstraint(mesh, 0.5f);
        auto volumeHigh = GenerateVolumeConstraint(mesh, 0.9f);

        // Higher pressure coefficient should result in lower compliance
        // compliance = 1.0 - pressure
        fails += expect_near_f("volume_compliance_low", volumeLow.mCompliance, 0.7f, 0.0001f);
        fails += expect_near_f("volume_compliance_mid", volumeMid.mCompliance, 0.5f, 0.0001f);
        fails += expect_near_f("volume_compliance_high", volumeHigh.mCompliance, 0.1f, 0.0001f);

        // Verify compliance is in valid range [0, 1]
        if (volumeLow.mCompliance < 0.0f || volumeLow.mCompliance > 1.0f) {
            printf("constraints: volume compliance out of range: %.4f\n", volumeLow.mCompliance);
            fails++;
        }
    }

    // Test volume constraint with different mesh sizes
    {
        IcosphereMesh mesh0 = GenerateIcosphere(0, 1.0f);
        IcosphereMesh mesh1 = GenerateIcosphere(1, 1.0f);
        IcosphereMesh mesh2 = GenerateIcosphere(2, 1.0f);

        float pressure = 0.5f;
        auto vol0 = GenerateVolumeConstraint(mesh0, pressure);
        auto vol1 = GenerateVolumeConstraint(mesh1, pressure);
        auto vol2 = GenerateVolumeConstraint(mesh2, pressure);

        // All should have same compliance regardless of mesh detail
        fails += expect_near_f("volume_invariant_0", vol0.mCompliance, 0.5f, 0.0001f);
        fails += expect_near_f("volume_invariant_1", vol1.mCompliance, 0.5f, 0.0001f);
        fails += expect_near_f("volume_invariant_2", vol2.mCompliance, 0.5f, 0.0001f);
    }

    // Test edge constraint uniqueness (no duplicate edges)
    {
        IcosphereMesh mesh = GenerateIcosphere(0, 1.0f);
        auto edges = GenerateEdgeConstraints(mesh, ConstraintPresets::Amoeba);

        // Check for duplicate edges
        for (size_t i = 0; i < edges.size(); i++) {
            for (size_t j = i + 1; j < edges.size(); j++) {
                bool duplicate = (edges[i].mVertex[0] == edges[j].mVertex[0] &&
                                  edges[i].mVertex[1] == edges[j].mVertex[1]) ||
                                 (edges[i].mVertex[0] == edges[j].mVertex[1] &&
                                  edges[i].mVertex[1] == edges[j].mVertex[0]);
                if (duplicate) {
                    printf("constraints: duplicate edge found (%u, %u)\n",
                           edges[i].mVertex[0], edges[i].mVertex[1]);
                    fails++;
                }
            }
        }
    }

    if (fails == 0) {
        printf("constraints: All tests passed\n");
    }

    return fails;
}
