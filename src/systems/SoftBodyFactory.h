#ifndef MICRO_IDLE_SOFTBODY_FACTORY_H
#define MICRO_IDLE_SOFTBODY_FACTORY_H

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyID.h>
#include "raylib.h"

namespace micro_idle {

struct PhysicsSystemState; // Forward declaration

/**
 * Factory for creating Jolt soft body entities
 *
 * Uses the Puppet architecture: single Jolt soft body for physics simulation,
 * with vertex positions extracted for SDF raymarching rendering.
 * Forces are applied directly to soft body vertices for EC&M locomotion.
 */
class SoftBodyFactory {
public:
    /**
     * Create an amoeba soft body using proper Jolt soft body physics
     *
     * @param physics The physics system
     * @param position Center position
     * @param radius Approximate radius
     * @param subdivisions Icosphere subdivisions (0=12 verts, 1=42 verts, 2=162 verts)
     * @return Jolt BodyID for the created soft body
     */
    static JPH::BodyID CreateAmoeba(
        PhysicsSystemState* physics,
        Vector3 position,
        float radius,
        int subdivisions = 1
    );

    /**
     * Extract vertex positions from a soft body for SDF rendering
     *
     * @param physics The physics system
     * @param bodyID The soft body BodyID
     * @param outPositions Output array (must be pre-allocated)
     * @param maxPositions Maximum number of positions to extract
     * @return Number of vertices extracted
     */
    static int ExtractVertexPositions(
        PhysicsSystemState* physics,
        JPH::BodyID bodyID,
        Vector3* outPositions,
        int maxPositions
    );

    /**
     * Get vertex count for a soft body
     *
     * @param physics The physics system
     * @param bodyID The soft body BodyID
     * @return Number of vertices in the soft body
     */
    static int GetVertexCount(
        PhysicsSystemState* physics,
        JPH::BodyID bodyID
    );
};

} // namespace micro_idle

#endif
