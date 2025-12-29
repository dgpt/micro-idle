#version 330

// Metaball Fragment Shader
// Renders soft spheres that blend together with additive blending

// Inputs from vertex shader
in vec2 fragTexCoord;
in vec3 fragParticleCenter;
in float fragParticleRadius;
in vec4 fragColor;

// Output
out vec4 finalColor;

void main() {
    // Compute distance from billboard center (in normalized coords)
    vec2 center = vec2(0.5, 0.5);
    float dist = distance(fragTexCoord, center);

    // Normalize distance (0 at center, 1 at edge)
    float normalizedDist = dist / 0.5;

    // Soft sphere falloff
    if (normalizedDist > 1.0) {
        discard;  // Outside the circle
    }

    // Smooth alpha falloff (soft edges)
    float alpha = 1.0 - smoothstep(0.7, 1.0, normalizedDist);

    // Brighten center, darken edges for 3D sphere effect
    float brightness = 1.0 - (normalizedDist * 0.3);

    vec3 color = fragColor.rgb * brightness;

    // Output with soft alpha - will blend additively with other particles
    finalColor = vec4(color, alpha * fragColor.a * 0.8);
}
