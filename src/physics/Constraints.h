#ifndef MICRO_IDLE_CONSTRAINTS_H
#define MICRO_IDLE_CONSTRAINTS_H

#include <Jolt/Jolt.h>
#include <Jolt/Physics/SoftBody/SoftBodyCreationSettings.h>
#include "Icosphere.h"
#include <vector>
#include <utility>

namespace micro_idle {

/**
 * Constraint configuration for soft bodies
 */
struct ConstraintConfig {
    float compliance;           // How much the constraint can stretch (0 = rigid, higher = softer)
    float damping;             // Energy dissipation (0 = bouncy, 1 = fully damped)
};

/**
 * Generate edge distance constraints from icosphere mesh
 *
 * Creates distance constraints along all edges to maintain structural integrity
 *
 * @param mesh The icosphere mesh
 * @param config Constraint stiffness and damping
 * @return Vector of SoftBodySharedSettings::Edge for Jolt soft body creation
 */
std::vector<JPH::SoftBodySharedSettings::Edge> GenerateEdgeConstraints(
    const IcosphereMesh& mesh,
    const ConstraintConfig& config
);

/**
 * Generate volume constraint for soft body
 *
 * Creates a volume constraint to maintain internal pressure (prevents collapse)
 *
 * @param mesh The icosphere mesh
 * @param pressureCoefficient How much the body resists volume change (0.0 - 1.0)
 *                            0.0 = no resistance (can collapse)
 *                            1.0 = maximum resistance (very stiff)
 * @return SoftBodySharedSettings::Volume for Jolt soft body creation
 */
JPH::SoftBodySharedSettings::Volume GenerateVolumeConstraint(
    const IcosphereMesh& mesh,
    float pressureCoefficient
);

/**
 * Preset constraint configurations for different softness levels
 */
namespace ConstraintPresets {
    constexpr ConstraintConfig RigidSphere = {0.0f, 0.1f};      // Very stiff, minimal stretch
    constexpr ConstraintConfig SoftSphere = {0.001f, 0.3f};     // Moderate softness
    constexpr ConstraintConfig JellySphere = {0.01f, 0.5f};     // Very soft, gel-like
    constexpr ConstraintConfig Amoeba = {0.008f, 0.8f};          // Amoeba-like (gelled, higher damping for stability)
}

} // namespace micro_idle

#endif
