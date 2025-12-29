#include "src/physics/Icosphere.h"
#include <stdio.h>
#include <cmath>

using namespace micro_idle;

static int expect_equal_i(const char* label, int a, int expected) {
    if (a != expected) {
        printf("icosphere %s: expected %d, got %d\n", label, expected, a);
        return 1;
    }
    return 0;
}

static int expect_near_f(const char* label, float a, float expected, float epsilon) {
    float diff = fabsf(a - expected);
    if (diff > epsilon) {
        printf("icosphere %s: expected %.4f, got %.4f (diff %.6f)\n", label, expected, a, diff);
        return 1;
    }
    return 0;
}

int test_icosphere_run(void) {
    int fails = 0;

    // Test subdivision 0 (base icosahedron)
    {
        IcosphereMesh mesh = GenerateIcosphere(0, 1.0f);
        fails += expect_equal_i("subdiv0_vertices", mesh.vertexCount, 12);
        fails += expect_equal_i("subdiv0_triangles", mesh.triangleCount, 20);

        // Verify all vertices are on unit sphere surface
        for (int i = 0; i < mesh.vertexCount; i++) {
            Vector3 v = mesh.vertices[i];
            float length = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
            fails += expect_near_f("subdiv0_vertex_radius", length, 1.0f, 0.0001f);
        }

        // Verify triangle indices are valid
        for (int i = 0; i < mesh.triangleCount * 3; i++) {
            int idx = mesh.triangles[i];
            if (idx < 0 || idx >= mesh.vertexCount) {
                printf("icosphere subdiv0: invalid triangle index %d (vertexCount=%d)\n", idx, mesh.vertexCount);
                fails++;
            }
        }
    }

    // Test subdivision 1
    {
        IcosphereMesh mesh = GenerateIcosphere(1, 1.0f);
        fails += expect_equal_i("subdiv1_vertices", mesh.vertexCount, 42);
        fails += expect_equal_i("subdiv1_triangles", mesh.triangleCount, 80);

        // Verify all vertices are on unit sphere surface
        for (int i = 0; i < mesh.vertexCount; i++) {
            Vector3 v = mesh.vertices[i];
            float length = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
            fails += expect_near_f("subdiv1_vertex_radius", length, 1.0f, 0.0001f);
        }
    }

    // Test subdivision 2
    {
        IcosphereMesh mesh = GenerateIcosphere(2, 1.0f);
        fails += expect_equal_i("subdiv2_vertices", mesh.vertexCount, 162);
        fails += expect_equal_i("subdiv2_triangles", mesh.triangleCount, 320);
    }

    // Test custom radius
    {
        float testRadius = 2.5f;
        IcosphereMesh mesh = GenerateIcosphere(0, testRadius);

        for (int i = 0; i < mesh.vertexCount; i++) {
            Vector3 v = mesh.vertices[i];
            float length = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
            fails += expect_near_f("custom_radius_vertex", length, testRadius, 0.0001f);
        }
    }

    // Test edge generation
    {
        IcosphereMesh mesh = GenerateIcosphere(0, 1.0f);
        auto edges = GenerateEdges(mesh);

        // Base icosahedron has 30 edges
        fails += expect_equal_i("subdiv0_edges", (int)edges.size(), 30);

        // Verify edge indices are valid
        for (const auto& edge : edges) {
            if (edge.first < 0 || edge.first >= mesh.vertexCount ||
                edge.second < 0 || edge.second >= mesh.vertexCount) {
                printf("icosphere: invalid edge (%d, %d)\n", edge.first, edge.second);
                fails++;
            }

            // Verify consistent ordering (first < second)
            if (edge.first >= edge.second) {
                printf("icosphere: edge not properly ordered (%d, %d)\n", edge.first, edge.second);
                fails++;
            }
        }
    }

    // Test edge generation for subdivision 1
    {
        IcosphereMesh mesh = GenerateIcosphere(1, 1.0f);
        auto edges = GenerateEdges(mesh);

        // Subdivision 1 should have 120 edges
        fails += expect_equal_i("subdiv1_edges", (int)edges.size(), 120);
    }

    if (fails == 0) {
        printf("icosphere: All tests passed\n");
    }

    return fails;
}
