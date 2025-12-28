#version 430 core

// Metaball Field Pass - Fragment Shader
// Outputs field contribution from this particle (Gaussian falloff)

in vec2 vBillboardUV;
in float vMicrobeID;
in vec3 vParticleWorldPos;
in float vInfluenceRadius;

layout(location = 0) out vec4 outField;

void main() {
    // Simple circular falloff
    float dist = length(vBillboardUV);
    if (dist > 1.0) discard;

    // Smooth falloff for membrane blending
    float r = dist;
    float field = 1.0 - r * r;  // Quadratic falloff
    field = max(0.0, field);

    // Strong field so membrane particles merge into one smooth boundary
    field *= 15.0;

    // Weight microbe ID by field strength for proper accumulation
    float weighted_id = field * vMicrobeID;

    outField = vec4(field, weighted_id, 0.0, 1.0);
}
