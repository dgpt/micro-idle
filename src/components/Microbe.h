#ifndef MICRO_IDLE_MICROBE_H
#define MICRO_IDLE_MICROBE_H

#include "raylib.h"
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyID.h>
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

// Microbe statistics and properties
struct MicrobeStats {
    float seed;                 // Procedural variation seed
    float baseRadius;           // Base size (before deformation)
    Color color;
    float health;
    float energy;
};

// Soft body structure - single Jolt soft body (Puppet architecture)
struct SoftBody {
    JPH::BodyID bodyID;         // Single Jolt soft body
    int vertexCount;            // Number of vertices in soft body
    int subdivisions;           // Icosphere subdivisions used
};

// Internal rigid skeleton - drives locomotion via friction-based grip-and-stretch
// Skeleton nodes are rigid spheres inside the soft body that push against the skin
struct InternalSkeleton {
    std::vector<JPH::BodyID> skeletonBodyIDs;  // Rigid sphere bodies inside soft body
    int skeletonNodeCount;                      // Number of skeleton nodes
};

// Microbe entity - combines all microbe-specific components
struct Microbe {
    MicrobeType type;
    MicrobeStats stats;
    SoftBody softBody;          // Jolt soft body (physics simulation)
};

} // namespace components

#endif
