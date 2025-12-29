#ifndef MICRO_IDLE_MICROBE_H
#define MICRO_IDLE_MICROBE_H

#include "raylib.h"
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Constraints/Constraint.h>
#include <vector>

namespace components {

// Microbe types from README.md design
enum class MicrobeType {
    // Protists
    Amoeba,
    Stentor,
    Lacrymaria,
    Vorticella,
    Didinium,
    Heliozoa,
    Radiolarian,
    Diatom,

    // Bacteria
    Coccus,
    Bacillus,
    Vibrio,
    Spirillum,

    // Viruses
    Icosahedral,
    Bacteriophage
};

// EC&M (Excitable Cortex & Memory) locomotion state
struct ECMLocomotion {
    float phase;                // 0-1 cycle position in 12-second cycle
    int targetParticleIndex;    // Which particle is extending as pseudopod
    Vector3 targetDirection;    // Direction of pseudopod extension
    float wigglePhase;          // For lateral wiggle motion
    bool isExtending;           // True during extension phase
    bool isSearching;           // True during search/wiggle phase
    bool isRetracting;          // True during retraction phase
};

// Microbe statistics and properties
struct MicrobeStats {
    float seed;                 // Procedural variation seed
    float baseRadius;           // Base size (before deformation)
    Color color;
    float health;
    float energy;
};

// Mesh topology for membrane
struct MembraneMesh {
    std::vector<int> triangleIndices;
    int meshVertexStartIndex;
    int meshVertexCount;
};

// Soft body structure - collection of connected particles
struct SoftBody {
    std::vector<JPH::BodyID> particleBodyIDs;  // Jolt bodies for each particle
    std::vector<Vector3> restPositions;         // Rest positions in local space
    std::vector<JPH::Constraint*> constraints;  // Distance constraints between particles
    int skeletonParticleCount;                  // Number of internal skeleton particles
    MembraneMesh membrane;                      // Elastic mesh membrane
};

// Microbe entity - combines all microbe-specific components
struct Microbe {
    MicrobeType type;
    MicrobeStats stats;
    SoftBody softBody;
    ECMLocomotion locomotion;
};

} // namespace components

#endif
