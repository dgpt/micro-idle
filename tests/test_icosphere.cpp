#include "src/physics/Icosphere.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>

using namespace micro_idle;
using Catch::Approx;

TEST_CASE("Icosphere - subdivision 0 (base icosahedron)", "[icosphere]") {
    IcosphereMesh mesh = GenerateIcosphere(0, 1.0f);

    REQUIRE(mesh.vertexCount == 12);
    REQUIRE(mesh.triangleCount == 20);

    // Verify all vertices are on unit sphere surface
    for (int i = 0; i < mesh.vertexCount; i++) {
        Vector3 v = mesh.vertices[i];
        float length = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
        REQUIRE(length == Approx(1.0f).margin(0.0001f));
    }

    // Verify triangle indices are valid
    for (int i = 0; i < mesh.triangleCount * 3; i++) {
        int idx = mesh.triangles[i];
        REQUIRE(idx >= 0);
        REQUIRE(idx < mesh.vertexCount);
    }
}

TEST_CASE("Icosphere - subdivision 1", "[icosphere]") {
    IcosphereMesh mesh = GenerateIcosphere(1, 1.0f);

    REQUIRE(mesh.vertexCount == 42);
    REQUIRE(mesh.triangleCount == 80);

    // Verify all vertices are on unit sphere surface
    for (int i = 0; i < mesh.vertexCount; i++) {
        Vector3 v = mesh.vertices[i];
        float length = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
        REQUIRE(length == Approx(1.0f).margin(0.0001f));
    }
}

TEST_CASE("Icosphere - subdivision 2", "[icosphere]") {
    IcosphereMesh mesh = GenerateIcosphere(2, 1.0f);

    REQUIRE(mesh.vertexCount == 162);
    REQUIRE(mesh.triangleCount == 320);
}

TEST_CASE("Icosphere - custom radius", "[icosphere]") {
    float testRadius = 2.5f;
    IcosphereMesh mesh = GenerateIcosphere(0, testRadius);

    for (int i = 0; i < mesh.vertexCount; i++) {
        Vector3 v = mesh.vertices[i];
        float length = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
        REQUIRE(length == Approx(testRadius).margin(0.0001f));
    }
}

TEST_CASE("Icosphere - edge generation subdivision 0", "[icosphere]") {
    IcosphereMesh mesh = GenerateIcosphere(0, 1.0f);
    auto edges = GenerateEdges(mesh);

    // Base icosahedron has 30 edges
    REQUIRE(edges.size() == 30);

    // Verify edge indices are valid
    for (const auto& edge : edges) {
        REQUIRE(edge.first >= 0);
        REQUIRE(edge.first < mesh.vertexCount);
        REQUIRE(edge.second >= 0);
        REQUIRE(edge.second < mesh.vertexCount);

        // Verify consistent ordering (first < second)
        REQUIRE(edge.first < edge.second);
    }
}

TEST_CASE("Icosphere - edge generation subdivision 1", "[icosphere]") {
    IcosphereMesh mesh = GenerateIcosphere(1, 1.0f);
    auto edges = GenerateEdges(mesh);

    // Subdivision 1 should have 120 edges
    REQUIRE(edges.size() == 120);
}
