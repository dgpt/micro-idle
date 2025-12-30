#include "src/physics/Constraints.h"
#include "src/physics/Icosphere.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>

using namespace micro_idle;
using Catch::Approx;

TEST_CASE("Constraints - edge generation subdivision 0", "[constraints]") {
    IcosphereMesh mesh = GenerateIcosphere(0, 1.0f);
    auto edges = GenerateEdgeConstraints(mesh, ConstraintPresets::Amoeba);

    // Base icosahedron has 30 edges
    REQUIRE(edges.size() == 30);

    // Verify all edge indices are valid
    for (const auto& edge : edges) {
        REQUIRE(edge.mVertex[0] < (JPH::uint32)mesh.vertexCount);
        REQUIRE(edge.mVertex[1] < (JPH::uint32)mesh.vertexCount);

        // Verify vertices are different
        REQUIRE(edge.mVertex[0] != edge.mVertex[1]);
    }

    // Verify compliance matches preset
    for (const auto& edge : edges) {
        REQUIRE(edge.mCompliance == Approx(ConstraintPresets::Amoeba.compliance).margin(0.0001f));
    }
}

TEST_CASE("Constraints - edge generation subdivision 1", "[constraints]") {
    IcosphereMesh mesh = GenerateIcosphere(1, 1.0f);
    auto edges = GenerateEdgeConstraints(mesh, ConstraintPresets::SoftSphere);

    // Subdivision 1 has 120 edges
    REQUIRE(edges.size() == 120);

    // Verify compliance matches preset
    if (edges.size() > 0) {
        REQUIRE(edges[0].mCompliance == Approx(ConstraintPresets::SoftSphere.compliance).margin(0.0001f));
    }
}

TEST_CASE("Constraints - rigid sphere preset", "[constraints]") {
    IcosphereMesh mesh = GenerateIcosphere(0, 1.0f);
    auto rigidEdges = GenerateEdgeConstraints(mesh, ConstraintPresets::RigidSphere);

    if (rigidEdges.size() > 0) {
        REQUIRE(rigidEdges[0].mCompliance == Approx(0.0f).margin(0.0001f));
    }
}

TEST_CASE("Constraints - jelly sphere preset", "[constraints]") {
    IcosphereMesh mesh = GenerateIcosphere(0, 1.0f);
    auto jellyEdges = GenerateEdgeConstraints(mesh, ConstraintPresets::JellySphere);

    if (jellyEdges.size() > 0) {
        REQUIRE(jellyEdges[0].mCompliance == Approx(0.01f).margin(0.0001f));
    }
}

TEST_CASE("Constraints - volume constraint compliance", "[constraints]") {
    IcosphereMesh mesh = GenerateIcosphere(0, 1.0f);

    // Test different pressure coefficients
    auto volumeLow = GenerateVolumeConstraint(mesh, 0.3f);
    auto volumeMid = GenerateVolumeConstraint(mesh, 0.5f);
    auto volumeHigh = GenerateVolumeConstraint(mesh, 0.9f);

    // Higher pressure coefficient should result in lower compliance
    // compliance = 1.0 - pressure
    REQUIRE(volumeLow.mCompliance == Approx(0.7f).margin(0.0001f));
    REQUIRE(volumeMid.mCompliance == Approx(0.5f).margin(0.0001f));
    REQUIRE(volumeHigh.mCompliance == Approx(0.1f).margin(0.0001f));

    // Verify compliance is in valid range [0, 1]
    REQUIRE(volumeLow.mCompliance >= 0.0f);
    REQUIRE(volumeLow.mCompliance <= 1.0f);
}

TEST_CASE("Constraints - volume constraint mesh size invariance", "[constraints]") {
    IcosphereMesh mesh0 = GenerateIcosphere(0, 1.0f);
    IcosphereMesh mesh1 = GenerateIcosphere(1, 1.0f);
    IcosphereMesh mesh2 = GenerateIcosphere(2, 1.0f);

    float pressure = 0.5f;
    auto vol0 = GenerateVolumeConstraint(mesh0, pressure);
    auto vol1 = GenerateVolumeConstraint(mesh1, pressure);
    auto vol2 = GenerateVolumeConstraint(mesh2, pressure);

    // All should have same compliance regardless of mesh detail
    REQUIRE(vol0.mCompliance == Approx(0.5f).margin(0.0001f));
    REQUIRE(vol1.mCompliance == Approx(0.5f).margin(0.0001f));
    REQUIRE(vol2.mCompliance == Approx(0.5f).margin(0.0001f));
}

TEST_CASE("Constraints - edge uniqueness (no duplicates)", "[constraints]") {
    IcosphereMesh mesh = GenerateIcosphere(0, 1.0f);
    auto edges = GenerateEdgeConstraints(mesh, ConstraintPresets::Amoeba);

    // Check for duplicate edges
    for (size_t i = 0; i < edges.size(); i++) {
        for (size_t j = i + 1; j < edges.size(); j++) {
            bool duplicate = (edges[i].mVertex[0] == edges[j].mVertex[0] &&
                              edges[i].mVertex[1] == edges[j].mVertex[1]) ||
                             (edges[i].mVertex[0] == edges[j].mVertex[1] &&
                              edges[i].mVertex[1] == edges[j].mVertex[0]);
            REQUIRE_FALSE(duplicate);
        }
    }
}
