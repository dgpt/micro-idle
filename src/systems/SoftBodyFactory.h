#ifndef MICRO_IDLE_SOFTBODY_FACTORY_H
#define MICRO_IDLE_SOFTBODY_FACTORY_H

#include "src/components/Microbe.h"
#include "src/systems/PhysicsSystem.h"
#include "raylib.h"

namespace micro_idle {

// Factory for creating soft body microbes
class SoftBodyFactory {
public:
    // Create an amoeba soft body (blob with deformable membrane)
    static components::SoftBody createAmoeba(
        PhysicsSystemState* physics,
        Vector3 centerPosition,
        float radius,
        int particleCount = 16
    );

    // Create distance constraints between particles for soft body physics
    static void createSpringConstraints(
        PhysicsSystemState* physics,
        components::SoftBody& softBody,
        float stiffness = 0.8f,
        float damping = 0.1f
    );

    // Destroy soft body and all its constraints
    static void destroySoftBody(
        PhysicsSystemState* physics,
        components::SoftBody& softBody
    );

private:
    // Helper: Create spherical particle distribution
    static std::vector<Vector3> generateSpherePoints(int count, float radius);

    // Helper: Generate icosphere mesh topology
    static void generateIcosphere(int subdivisions, float radius,
                                  std::vector<Vector3>& outVertices,
                                  std::vector<int>& outTriangles);
};

} // namespace micro_idle

#endif
