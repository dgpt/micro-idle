#version 430 core

// Fragment shader for XPBD microbes
// Clean, simple rendering focused on readable biological shapes.

in vec4 vColor;
in vec2 vLocal;
in float vType;
in float vSquish;
in float vSeed;

out vec4 fragColor;

uniform float u_time;

void main() {
    vec2 p = vLocal;
    float dist = length(p);

    // Discard outside unit circle
    if (dist > 1.0) discard;

    // Type-specific shape modifications
    float type = floor(vType + 0.5);
    float sdf = dist - 1.0;

    // Coccus: spherical with small cilia bumps
    if (type < 0.5) {
        float bumps = sin(atan(p.y, p.x) * 12.0 + vSeed * 6.28) * 0.08;
        sdf = dist - (1.0 + bumps * (1.0 - dist));
    }
    // Bacillus: rod shape with rounded caps
    else if (type < 1.5) {
        float rodLen = 0.4;
        float px = abs(p.x) - rodLen;
        if (px < 0.0) px = 0.0;
        sdf = length(vec2(px, p.y)) - 0.6;
    }
    // Vibrio: curved comma shape
    else if (type < 2.5) {
        float curve = p.x * 0.4;
        sdf = length(vec2(p.x, p.y - curve)) - 0.7 - p.x * 0.2;
    }
    // Spirillum: wavy/spiral
    else {
        float wave = sin(p.x * 4.0 + vSeed * 3.0) * 0.15;
        sdf = length(vec2(p.x, p.y - wave)) - 0.5;
    }

    // Soft edge
    float alpha = 1.0 - smoothstep(-0.1, 0.05, sdf);
    if (alpha < 0.01) discard;

    // Base color with subtle variation
    vec3 baseColor = vColor.rgb;

    // Membrane edge - slightly darker/more saturated
    float edge = smoothstep(-0.15, -0.02, sdf);
    vec3 edgeColor = baseColor * 0.7;
    vec3 color = mix(baseColor, edgeColor, edge * 0.6);

    // Center highlight (dome effect)
    float highlight = 1.0 - dist * 0.6;
    color = mix(color, baseColor * 1.3, highlight * 0.3);

    // Internal structure hint (darker regions)
    float internal = sin(p.x * 8.0 + vSeed * 10.0) * sin(p.y * 8.0 + vSeed * 7.0);
    internal = internal * 0.5 + 0.5;
    internal *= (1.0 - dist) * 0.15;
    color = mix(color, baseColor * 0.6, internal);

    // Squish effect: membrane becomes tighter/more visible when compressed
    if (vSquish > 0.1) {
        float tensionRing = smoothstep(0.7, 0.9, dist) * vSquish;
        color = mix(color, vec3(1.0), tensionRing * 0.3);
    }

    fragColor = vec4(color, alpha * vColor.a);
}
