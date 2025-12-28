#version 430 core

// Simple particle rendering - smooth circles with alpha

in vec2 vUV;
in vec4 vColor;

layout(location = 0) out vec4 outColor;

void main() {
    // Simple circular falloff
    float dist = length(vUV);
    if (dist > 1.0) discard;

    // Smooth falloff for nice blending
    float alpha = 1.0 - smoothstep(0.0, 1.0, dist);
    alpha = alpha * alpha * alpha;  // Cube for very soft edges

    outColor = vec4(vColor.rgb, alpha * 0.9);
}
