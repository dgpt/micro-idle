#version 430 core

// Fragment shader for XPBD microbes
// Realistic biological rendering with translucent membrane, organelles, and subtle lighting

in vec4 vColor;
in vec2 vLocal;
in float vType;
in float vSquish;
in float vSeed;

out vec4 fragColor;

uniform float u_time;

// Hash function for organic variation
float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

// Smooth noise for organic patterns
float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f); // smoothstep interpolation

    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));

    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

void main() {
    vec2 p = vLocal;
    float dist = length(p);

    // Discard outside unit circle
    if (dist > 1.0) discard;

    // Type-specific shape modifications
    float type = floor(vType + 0.5);
    float sdf = dist - 1.0;

    // Coccus: spherical with dynamic cilia bumps
    if (type < 0.5) {
        float angle = atan(p.y, p.x);
        float bumps = sin(angle * 12.0 + vSeed * 6.28 + u_time * 2.0) * 0.08;
        bumps += sin(angle * 8.0 - u_time * 1.5 + vSeed) * 0.04; // Secondary cilia motion
        sdf = dist - (1.0 + bumps * (1.0 - dist * 0.8));
    }
    // Bacillus: rod shape with subtle undulation
    else if (type < 1.5) {
        float rodLen = 0.4;
        float undulation = sin(p.y * 6.0 + u_time * 1.2 + vSeed * 3.0) * 0.03;
        float px = abs(p.x + undulation) - rodLen;
        if (px < 0.0) px = 0.0;
        sdf = length(vec2(px, p.y)) - 0.6;
    }
    // Vibrio: curved comma with flexing motion
    else if (type < 2.5) {
        float flex = sin(u_time * 1.8 + vSeed) * 0.1;
        float curve = p.x * (0.4 + flex);
        sdf = length(vec2(p.x, p.y - curve)) - 0.7 - p.x * 0.2;
    }
    // Spirillum: wavy/spiral with propagating wave
    else {
        float wave = sin(p.x * 4.0 + vSeed * 3.0 - u_time * 2.5) * 0.15;
        sdf = length(vec2(p.x, p.y - wave)) - 0.5;
    }

    // Soft, translucent edge
    float alpha = 1.0 - smoothstep(-0.12, 0.06, sdf);
    if (alpha < 0.01) discard;

    // === BASE COLOR ===
    vec3 baseColor = vColor.rgb;

    // === SUBSURFACE SCATTERING APPROXIMATION ===
    // Light penetrates through the translucent membrane
    float thickness = 1.0 - dist; // Thinner at edges
    vec3 subsurface = baseColor * 1.4 * thickness;

    // === MEMBRANE RENDERING ===
    // Translucent gel-like membrane with color variation
    float membrane_dist = smoothstep(-0.15, -0.02, sdf);

    // Membrane has slightly different hue (more saturated, slightly darker)
    vec3 membrane_color = baseColor * vec3(0.85, 0.9, 1.05);
    membrane_color = normalize(membrane_color) * length(baseColor) * 0.8;

    // Membrane thickness variation (organic irregularity)
    float membrane_noise = noise(p * 20.0 + vSeed * 10.0 + u_time * 0.3);
    membrane_color *= 0.9 + membrane_noise * 0.2;

    vec3 color = mix(subsurface, membrane_color, membrane_dist * 0.7);

    // === DOME LIGHTING ===
    // 3D lighting effect assuming dome shape and top-down lighting
    float dome_height = sqrt(max(0.0, 1.0 - dist * dist));
    float lighting = 0.6 + dome_height * 0.4; // Gentle lighting
    color *= lighting;

    // === INTERNAL ORGANELLES ===
    // Nucleus, ribosomes, and other internal structures
    vec2 internal_p = p * 3.0 + vec2(vSeed * 5.0, vSeed * 7.0);
    float organelles = 0.0;

    // Large nucleus-like structure (offset from center)
    vec2 nucleus_pos = vec2(sin(vSeed * 6.28) * 0.2, cos(vSeed * 6.28) * 0.2);
    float nucleus = smoothstep(0.35, 0.25, length(p - nucleus_pos));
    organelles += nucleus * 0.4;

    // Small ribosomes scattered throughout
    float ribosomes = noise(internal_p + u_time * 0.1);
    ribosomes = smoothstep(0.5, 0.7, ribosomes);
    organelles += ribosomes * (1.0 - dist) * 0.2;

    // Internal structures are darker
    color = mix(color, baseColor * 0.5, organelles * (1.0 - membrane_dist));

    // === CYTOPLASM STREAMING ===
    // Subtle flowing patterns inside the cell
    vec2 flow_p = p * 6.0 + vec2(u_time * 0.5, u_time * 0.3);
    float flow = noise(flow_p + vSeed);
    flow = (flow - 0.5) * 0.08;
    color += baseColor * flow * (1.0 - dist) * (1.0 - membrane_dist);

    // === PULSING / BREATHING ===
    // Subtle size pulsation simulating metabolic activity
    float pulse = sin(u_time * 1.5 + vSeed * 6.28) * 0.03 + 1.0;
    color *= pulse;

    // === SQUISH EFFECT ===
    // Membrane tension when compressed - becomes more opaque and lighter
    if (vSquish > 0.05) {
        float tension = smoothstep(0.65, 0.85, dist) * vSquish;
        // Tension creates visible stress in membrane
        color = mix(color, vec3(1.0, 1.0, 1.0) * baseColor, tension * 0.4);
        alpha = mix(alpha, 1.0, tension * 0.3); // More opaque under stress
    }

    // === EDGE GLOW ===
    // Slight bioluminescent glow at the very edge (optional)
    float edge_glow = smoothstep(0.92, 0.98, dist) * (1.0 - membrane_dist);
    color += baseColor * 0.3 * edge_glow;

    // Final alpha with translucency
    alpha *= vColor.a;

    // Make center slightly more transparent for gel-like appearance
    alpha *= 0.7 + dist * 0.3;

    fragColor = vec4(color, alpha);
}
