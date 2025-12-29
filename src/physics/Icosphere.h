#ifndef MICRO_IDLE_ICOSPHERE_H
#define MICRO_IDLE_ICOSPHERE_H

#include "raylib.h"
#include <vector>

namespace micro_idle {

/**
 * Icosphere mesh data
 *
 * An icosphere is a geodesic polyhedron created by subdividing an icosahedron
 * and projecting vertices onto a sphere surface.
 */
struct IcosphereMesh {
    std::vector<Vector3> vertices;      // Vertex positions
    std::vector<int> triangles;         // Triangle indices (groups of 3)
    int vertexCount;                    // Number of vertices
    int triangleCount;                  // Number of triangles
};

/**
 * Generate an icosphere mesh
 *
 * @param subdivisions Number of subdivision iterations (0-3 recommended)
 *                     0 = 12 vertices, 20 triangles (base icosahedron)
 *                     1 = 42 vertices, 80 triangles
 *                     2 = 162 vertices, 320 triangles
 * @param radius Radius of the sphere
 * @return IcosphereMesh with normalized vertices
 */
IcosphereMesh GenerateIcosphere(int subdivisions, float radius);

/**
 * Generate edge pairs from triangle mesh
 * Used for creating distance constraints in soft bodies
 *
 * @param mesh The icosphere mesh
 * @return Vector of edge pairs (vertex index pairs)
 */
std::vector<std::pair<int, int>> GenerateEdges(const IcosphereMesh& mesh);

} // namespace micro_idle

#endif
