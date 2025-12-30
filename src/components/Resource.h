#ifndef MICRO_IDLE_RESOURCE_H
#define MICRO_IDLE_RESOURCE_H

#include "raylib.h"

namespace components {

// Resource types from README.md design
// Each resource is unlocked by a DNA trait
enum class ResourceType {
    Sodium,              // Unlocked by Capsule trait
    Glucose,             // Unlocked by Flagella trait
    Iron,                // Unlocked by Pili trait
    Calcium,             // Unlocked by Endospores trait
    Lipids,              // Unlocked by LPS Layer trait
    Oxygen,              // Unlocked by Photosynthesis trait
    SignalingMolecules   // Unlocked by Quorum Sensing trait
};

// Resource component - represents a resource drop in the world
struct Resource {
    ResourceType type;
    float amount;        // Amount of resource (can be fractional)
    float lifetime;      // Time remaining before despawn (seconds)
    float maxLifetime;   // Maximum lifetime (for visual feedback)
    Color color;         // Visual color for the resource
    bool isCollected;    // Whether resource has been collected
};

// Resource inventory - singleton component tracking player's resources
struct ResourceInventory {
    float sodium{0.0f};
    float glucose{0.0f};
    float iron{0.0f};
    float calcium{0.0f};
    float lipids{0.0f};
    float oxygen{0.0f};
    float signalingMolecules{0.0f};

    // Get resource amount by type
    float get(ResourceType type) const {
        switch (type) {
            case ResourceType::Sodium: return sodium;
            case ResourceType::Glucose: return glucose;
            case ResourceType::Iron: return iron;
            case ResourceType::Calcium: return calcium;
            case ResourceType::Lipids: return lipids;
            case ResourceType::Oxygen: return oxygen;
            case ResourceType::SignalingMolecules: return signalingMolecules;
            default: return 0.0f;
        }
    }

    // Add resource amount
    void add(ResourceType type, float amount) {
        switch (type) {
            case ResourceType::Sodium: sodium += amount; break;
            case ResourceType::Glucose: glucose += amount; break;
            case ResourceType::Iron: iron += amount; break;
            case ResourceType::Calcium: calcium += amount; break;
            case ResourceType::Lipids: lipids += amount; break;
            case ResourceType::Oxygen: oxygen += amount; break;
            case ResourceType::SignalingMolecules: signalingMolecules += amount; break;
        }
    }
};

} // namespace components

#endif
