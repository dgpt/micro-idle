#include "game/microbe_bodies.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Helper to create ring of positions
static void addRing(std::vector<Vec3>& positions, int count, float radius, float y = 0.0f) {
    for (int i = 0; i < count; i++) {
        float angle = (2.0f * M_PI * i) / count;
        positions.push_back(Vec3(
            cosf(angle) * radius,
            y,
            sinf(angle) * radius
        ));
    }
}

// Helper to add distance constraint
static void addConstraint(std::vector<ConstraintDef>& constraints,
                         int n1, int n2, float restLength, float stiffness) {
    constraints.push_back({n1, n2, restLength, stiffness});
}

// AMOEBA: Blob-like with thin pseudopod extension, highly deformable
MicrobeBodyPlan getAmoebaPlan() {
    MicrobeBodyPlan plan;
    plan.skeletonCount = 16;
    plan.membraneCount = 16;
    plan.defaultStiffness = 200.0f;
    plan.damping = 0.1f;
    plan.particleMass = 1.0f;

    // Skeleton: 3 concentric rings (1 center + 5 inner + 10 outer)
    plan.skeletonPos.push_back(Vec3(0, 0, 0));  // Center
    addRing(plan.skeletonPos, 5, 0.5f);          // Inner ring
    addRing(plan.skeletonPos, 10, 1.0f);         // Outer ring

    // Membrane: Outer elastic circle
    addRing(plan.membranePos, 16, 1.5f);

    // Constraints - Skeleton (stiff, shape-maintaining)
    int center = 0;
    int innerStart = 1;
    int outerStart = 6;

    // Center to inner ring (radial spokes)
    for (int i = 0; i < 5; i++) {
        addConstraint(plan.constraints, center, innerStart + i, 0.5f, plan.defaultStiffness * 2.0f);
    }

    // Inner ring adjacency
    for (int i = 0; i < 5; i++) {
        int next = (i + 1) % 5;
        addConstraint(plan.constraints, innerStart + i, innerStart + next, 0.588f, plan.defaultStiffness * 1.5f);
    }

    // Inner to outer ring (spokes)
    for (int i = 0; i < 5; i++) {
        // Each inner connects to 2 outer nodes
        int outer1 = outerStart + (i * 2);
        int outer2 = outerStart + (i * 2 + 1) % 10;
        addConstraint(plan.constraints, innerStart + i, outer1, 0.559f, plan.defaultStiffness * 1.5f);
        addConstraint(plan.constraints, innerStart + i, outer2, 0.559f, plan.defaultStiffness * 1.5f);
    }

    // Outer ring adjacency
    for (int i = 0; i < 10; i++) {
        int next = (i + 1) % 10;
        addConstraint(plan.constraints, outerStart + i, outerStart + next, 0.618f, plan.defaultStiffness);
    }

    // Membrane constraints (elastic, deformable)
    float membraneStiffness = plan.defaultStiffness * 0.3f;  // Softer

    // Membrane ring adjacency
    for (int i = 0; i < 16; i++) {
        int next = (i + 1) % 16;
        int m1 = plan.skeletonCount + i;
        int m2 = plan.skeletonCount + next;
        addConstraint(plan.constraints, m1, m2, 0.589f, membraneStiffness);
    }

    // Attachment: skeleton outer ring to membrane
    float attachStiffness = plan.defaultStiffness * 0.5f;  // Moderate coupling
    for (int i = 0; i < 10; i++) {
        // Each outer skeleton node attaches to closest membrane nodes
        int memIdx1 = plan.skeletonCount + (i * 16 / 10);
        int memIdx2 = plan.skeletonCount + ((i * 16 / 10) + 1) % 16;
        addConstraint(plan.constraints, outerStart + i, memIdx1, 0.5f, attachStiffness);
        addConstraint(plan.constraints, outerStart + i, memIdx2, 0.5f, attachStiffness);
    }

    return plan;
}

// STENTOR: Trumpet-shaped ciliate with contractile body
MicrobeBodyPlan getStenorPlan() {
    MicrobeBodyPlan plan;
    plan.skeletonCount = 20;
    plan.membraneCount = 12;
    plan.defaultStiffness = 300.0f;  // More rigid than amoeba
    plan.damping = 0.15f;
    plan.particleMass = 1.2f;

    // Trumpet shape: wider at top, narrow at base
    for (int layer = 0; layer < 4; layer++) {
        float y = layer * 0.5f;
        float radius = 1.5f - (layer * 0.3f);  // Tapering
        int count = (layer == 0) ? 8 : 4;
        addRing(plan.skeletonPos, count, radius, y);
    }

    // Membrane: follows trumpet shape
    addRing(plan.membranePos, 8, 1.7f, 0.0f);   // Wide top
    addRing(plan.membranePos, 4, 0.9f, 1.5f);   // Narrow base

    // TODO Phase 2.4: Add constraints for trumpet structure
    plan.defaultStiffness = 300.0f;

    return plan;
}

// LACRYMARIA: Body with extremely long extendable neck
MicrobeBodyPlan getLacrymariaPlan() {
    MicrobeBodyPlan plan;
    plan.skeletonCount = 46;  // 16 body + 30 neck
    plan.membraneCount = 16;
    plan.defaultStiffness = 250.0f;
    plan.damping = 0.08f;  // Low damping for flexible neck
    plan.particleMass = 0.8f;

    // Body: spherical base (same as amoeba)
    plan.skeletonPos.push_back(Vec3(0, 0, 0));
    addRing(plan.skeletonPos, 5, 0.5f);
    addRing(plan.skeletonPos, 10, 1.0f);

    // Neck: linear chain extending outward
    for (int i = 0; i < 30; i++) {
        float t = (i + 1) * 0.3f;
        plan.skeletonPos.push_back(Vec3(t, 0, 0));
    }

    // Membrane: wraps body only
    addRing(plan.membranePos, 16, 1.5f);

    // TODO Phase 2.4: Add constraints for neck chain
    plan.defaultStiffness = 250.0f;

    return plan;
}

// Stub implementations for remaining types (Phase 4)
MicrobeBodyPlan getVorticellaPlan() {
    MicrobeBodyPlan plan;
    plan.skeletonCount = 22;
    plan.membraneCount = 10;
    plan.defaultStiffness = 400.0f;
    plan.damping = 0.2f;
    plan.particleMass = 1.5f;
    // TODO Phase 4: Bell + contractile stalk
    return plan;
}

MicrobeBodyPlan getDidiniumPlan() {
    MicrobeBodyPlan plan;
    plan.skeletonCount = 22;
    plan.membraneCount = 12;
    plan.defaultStiffness = 500.0f;
    plan.damping = 0.25f;
    plan.particleMass = 2.0f;
    // TODO Phase 4: Barrel + proboscis
    return plan;
}

MicrobeBodyPlan getHeliozoaPlan() {
    MicrobeBodyPlan plan;
    plan.skeletonCount = 68;  // 8 core + 60 axopodia (12 spikes × 5 nodes)
    plan.membraneCount = 8;
    plan.defaultStiffness = 300.0f;
    plan.damping = 0.1f;
    plan.particleMass = 0.5f;
    // TODO Phase 4: Spherical core + radiating spikes
    return plan;
}

MicrobeBodyPlan getRadiolariaPlan() {
    MicrobeBodyPlan plan;
    plan.skeletonCount = 32;  // Icosahedral skeleton
    plan.membraneCount = 20;
    plan.defaultStiffness = 800.0f;  // Very rigid
    plan.damping = 0.3f;
    plan.particleMass = 3.0f;
    // TODO Phase 4: Geometric silica skeleton
    return plan;
}

MicrobeBodyPlan getDiatomPlan() {
    MicrobeBodyPlan plan;
    plan.skeletonCount = 8;  // Box corners
    plan.membraneCount = 0;  // No soft membrane
    plan.defaultStiffness = 10000.0f;  // Rigid frustule
    plan.damping = 0.5f;
    plan.particleMass = 5.0f;
    // TODO Phase 4: Rigid box structure
    return plan;
}

// Factory function
MicrobeBodyPlan getBodyPlan(MicrobeType type) {
    switch (type) {
        case MicrobeType::AMOEBA:       return getAmoebaPlan();
        case MicrobeType::STENTOR:      return getStenorPlan();
        case MicrobeType::LACRYMARIA:   return getLacrymariaPlan();
        case MicrobeType::VORTICELLA:   return getVorticellaPlan();
        case MicrobeType::DIDINIUM:     return getDidiniumPlan();
        case MicrobeType::HELIOZOA:     return getHeliozoaPlan();
        case MicrobeType::RADIOLARIAN:  return getRadiolariaPlan();
        case MicrobeType::DIATOM:       return getDiatomPlan();
        default:                        return getAmoebaPlan();  // Fallback
    }
}
