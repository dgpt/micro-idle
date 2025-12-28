#pragma once

#include "game/physics.h"
#include <vector>

// 3D vector helper
struct Vec3 {
    float x, y, z;
    Vec3(float x = 0, float y = 0, float z = 0) : x(x), y(y), z(z) {}
};

// Constraint definition for body plan
struct ConstraintDef {
    int node1;        // First node index
    int node2;        // Second node index
    float restLength; // Target distance
    float stiffness;  // Material stiffness (higher = more rigid)
};

// Complete body plan for a microbe type
struct MicrobeBodyPlan {
    // Particle layout
    int skeletonCount;                  // Number of internal structural nodes
    int membraneCount;                  // Number of outer elastic nodes
    std::vector<Vec3> skeletonPos;      // Skeleton positions (relative to center)
    std::vector<Vec3> membranePos;      // Membrane positions (relative to center)

    // Constraint network
    std::vector<ConstraintDef> constraints;

    // Material properties
    float defaultStiffness;             // Base stiffness value
    float damping;                      // Damping coefficient
    float particleMass;                 // Mass per particle
};

// Body plan factory functions
MicrobeBodyPlan getAmoebaPlan();
MicrobeBodyPlan getStenorPlan();
MicrobeBodyPlan getLacrymariaPlan();
MicrobeBodyPlan getVorticellaPlan();
MicrobeBodyPlan getDidiniumPlan();
MicrobeBodyPlan getHeliozoaPlan();
MicrobeBodyPlan getRadiolariaPlan();
MicrobeBodyPlan getDiatomPlan();

// Helper: Get body plan for a given microbe type
MicrobeBodyPlan getBodyPlan(MicrobeType type);
