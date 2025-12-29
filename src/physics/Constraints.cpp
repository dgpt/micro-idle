#include "Constraints.h"
#include <cmath>

namespace micro_idle {

std::vector<JPH::SoftBodySharedSettings::Edge> GenerateEdgeConstraints(
    const IcosphereMesh& mesh,
    const ConstraintConfig& config
) {
    std::vector<JPH::SoftBodySharedSettings::Edge> edges;

    // Get unique edges from mesh
    auto edgePairs = GenerateEdges(mesh);

    // Convert to Jolt edge format
    for (const auto& edgePair : edgePairs) {
        JPH::SoftBodySharedSettings::Edge edge;
        edge.mVertex[0] = (JPH::uint32)edgePair.first;
        edge.mVertex[1] = (JPH::uint32)edgePair.second;
        edge.mCompliance = config.compliance;

        edges.push_back(edge);
    }

    return edges;
}

JPH::SoftBodySharedSettings::Volume GenerateVolumeConstraint(
    const IcosphereMesh& mesh,
    float pressureCoefficient
) {
    JPH::SoftBodySharedSettings::Volume volume;

    // Set compliance based on pressure coefficient
    // Lower compliance = stiffer volume constraint
    volume.mCompliance = 1.0f - pressureCoefficient;

    // Note: Jolt's SoftBodySharedSettings::Volume uses face-based volume calculation
    // The soft body system will automatically compute volume from the mesh faces
    // We just need to provide the compliance parameter

    return volume;
}

} // namespace micro_idle
