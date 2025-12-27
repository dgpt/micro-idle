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

    // Smooth Gaussian-like falloff for better blending
    float r = dist;
    float field = 1.0 - r * r;  // Quadratic falloff
    field = max(0.0, field);

    // Field strength tuned for filled disc with 4 rings
    field *= 4.0;

    outField = vec4(field, 0.0, vMicrobeID / 1000.0, 1.0);
}
