#include "Icosphere.h"
#include <cmath>
#include <map>
#include <set>

namespace micro_idle {

// Helper: Normalize a vector
static Vector3 Normalize(Vector3 v) {
    float length = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
    if (length > 0.0f) {
        return {v.x / length, v.y / length, v.z / length};
    }
    return {0.0f, 0.0f, 0.0f};
}

// Helper: Get midpoint between two vertices and normalize to sphere surface
static int GetMidpoint(
    int v1, int v2,
    std::vector<Vector3>& vertices,
    std::map<std::pair<int, int>, int>& midpointCache
) {
    // Ensure consistent ordering for cache key
    std::pair<int, int> key = (v1 < v2) ? std::make_pair(v1, v2) : std::make_pair(v2, v1);

    // Check if we already computed this midpoint
    auto it = midpointCache.find(key);
    if (it != midpointCache.end()) {
        return it->second;
    }

    // Calculate midpoint
    Vector3 p1 = vertices[v1];
    Vector3 p2 = vertices[v2];
    Vector3 mid = {
        (p1.x + p2.x) / 2.0f,
        (p1.y + p2.y) / 2.0f,
        (p1.z + p2.z) / 2.0f
    };

    // Normalize to sphere surface
    mid = Normalize(mid);

    // Add to vertices and cache
    int index = (int)vertices.size();
    vertices.push_back(mid);
    midpointCache[key] = index;

    return index;
}

IcosphereMesh GenerateIcosphere(int subdivisions, float radius) {
    IcosphereMesh mesh;

    // Step 1: Create base icosahedron vertices (12 vertices)
    // Using golden ratio for perfect icosahedron
    const float phi = (1.0f + sqrtf(5.0f)) / 2.0f;  // Golden ratio
    const float a = 1.0f;
    const float b = phi;

    // 12 vertices of icosahedron (normalized)
    std::vector<Vector3> baseVertices = {
        {-a,  b,  0}, { a,  b,  0}, {-a, -b,  0}, { a, -b,  0},
        { 0, -a,  b}, { 0,  a,  b}, { 0, -a, -b}, { 0,  a, -b},
        { b,  0, -a}, { b,  0,  a}, {-b,  0, -a}, {-b,  0,  a}
    };

    // Normalize base vertices
    for (auto& v : baseVertices) {
        v = Normalize(v);
    }

    mesh.vertices = baseVertices;

    // Step 2: Create base icosahedron triangles (20 faces)
    std::vector<int> baseTriangles = {
        // 5 faces around point 0
        0, 11, 5,   0, 5, 1,    0, 1, 7,    0, 7, 10,   0, 10, 11,
        // 5 adjacent faces
        1, 5, 9,    5, 11, 4,   11, 10, 2,  10, 7, 6,   7, 1, 8,
        // 5 faces around point 3
        3, 9, 4,    3, 4, 2,    3, 2, 6,    3, 6, 8,    3, 8, 9,
        // 5 adjacent faces
        4, 9, 5,    2, 4, 11,   6, 2, 10,   8, 6, 7,    9, 8, 1
    };

    mesh.triangles = baseTriangles;

    // Step 3: Subdivide
    std::map<std::pair<int, int>, int> midpointCache;

    for (int i = 0; i < subdivisions; i++) {
        std::vector<int> newTriangles;

        // For each triangle, create 4 new triangles
        for (size_t j = 0; j < mesh.triangles.size(); j += 3) {
            int v1 = mesh.triangles[j];
            int v2 = mesh.triangles[j + 1];
            int v3 = mesh.triangles[j + 2];

            // Get midpoints
            int m12 = GetMidpoint(v1, v2, mesh.vertices, midpointCache);
            int m23 = GetMidpoint(v2, v3, mesh.vertices, midpointCache);
            int m31 = GetMidpoint(v3, v1, mesh.vertices, midpointCache);

            // Create 4 new triangles
            newTriangles.push_back(v1);  newTriangles.push_back(m12); newTriangles.push_back(m31);
            newTriangles.push_back(v2);  newTriangles.push_back(m23); newTriangles.push_back(m12);
            newTriangles.push_back(v3);  newTriangles.push_back(m31); newTriangles.push_back(m23);
            newTriangles.push_back(m12); newTriangles.push_back(m23); newTriangles.push_back(m31);
        }

        mesh.triangles = newTriangles;
        midpointCache.clear();
    }

    // Step 4: Scale to desired radius
    for (auto& v : mesh.vertices) {
        v.x *= radius;
        v.y *= radius;
        v.z *= radius;
    }

    // Set counts
    mesh.vertexCount = (int)mesh.vertices.size();
    mesh.triangleCount = (int)mesh.triangles.size() / 3;

    return mesh;
}

std::vector<std::pair<int, int>> GenerateEdges(const IcosphereMesh& mesh) {
    std::set<std::pair<int, int>> edgeSet;

    // Extract unique edges from triangles
    for (size_t i = 0; i < mesh.triangles.size(); i += 3) {
        int v1 = mesh.triangles[i];
        int v2 = mesh.triangles[i + 1];
        int v3 = mesh.triangles[i + 2];

        // Add three edges (ensure consistent ordering)
        auto addEdge = [&edgeSet](int a, int b) {
            if (a > b) std::swap(a, b);
            edgeSet.insert({a, b});
        };

        addEdge(v1, v2);
        addEdge(v2, v3);
        addEdge(v3, v1);
    }

    // Convert set to vector
    std::vector<std::pair<int, int>> edges(edgeSet.begin(), edgeSet.end());
    return edges;
}

} // namespace micro_idle
