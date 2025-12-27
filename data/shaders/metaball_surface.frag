#version 430 core

// Metaball Surface Pass - Fragment Shader
// Samples field texture, applies threshold, renders biological amoeba surface

in vec2 vTexCoord;

uniform sampler2D u_field_texture;
uniform float u_time;
uniform float u_threshold;

struct Microbe {
    vec4 center;
    vec4 color;
    vec4 params;  // x = type, y = stiffness, z = seed, w = squish
    vec4 aabb;
};

layout(std430, binding = 1) readonly buffer Microbes { Microbe microbes[]; };

out vec4 FragColor;

// Hash function for noise
float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);

    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));

    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

void main() {
    // Sample metaball field
    vec4 field_sample = texture(u_field_texture, vTexCoord);
    float field_value = field_sample.r;
    float microbe_id_norm = field_sample.b;

    // Apply threshold with smooth antialiasing transition
    float threshold_margin = 0.02;  // Smooth edge width
    float edge_distance = (field_value - u_threshold) / threshold_margin;

    // Hard cutoff far below threshold
    if (edge_distance < -1.0) {
        discard;
    }

    // Smooth antialiasing at the edge
    float edge_alpha = smoothstep(-1.0, 0.0, edge_distance);

    // Compute distance from threshold (for edge effects)
    float dist_from_surface = (field_value - u_threshold) / max(field_value, 0.001);
    dist_from_surface = clamp(dist_from_surface, 0.0, 1.0);

    // Get microbe color based on ID
    int m_id = int(microbe_id_norm * 1000.0 + 0.5);
    if (m_id < 0 || m_id >= microbes.length()) m_id = 0;

    Microbe m = microbes[m_id];
    vec3 base_color = m.color.rgb;
    float type = m.params.x;
    float seed = m.params.z;
    float squish = m.params.w;

    // === MEMBRANE APPEARANCE ===

    // 1. Base transparency - edges more transparent
    float base_alpha = mix(0.4, 0.9, dist_from_surface);

    // 2. Membrane texture (subtle irregularities)
    vec2 membrane_uv = vTexCoord * 30.0 + vec2(u_time * 0.05, u_time * 0.03);
    float membrane_noise = noise(membrane_uv) * 0.15;

    // 3. Subsurface scattering effect (light penetrating membrane)
    float thickness = 1.0 - dist_from_surface;
    vec3 scatter_color = base_color * 1.4;
    float scatter = thickness * thickness * 0.5;

    // 4. Internal cytoplasm detail (very subtle to prevent artifacts)
    vec2 cyto_uv = vTexCoord * 12.0 - vec2(u_time * 0.08, u_time * 0.05) + seed * 10.0;
    float cytoplasm = noise(cyto_uv) * 0.04;  // Minimal to avoid shadow lines

    // 5. Organelles disabled to prevent shadow line artifacts on overlap
    float organelles = 0.0;

    // 6. Edge glow (membrane electric potential)
    float edge_glow = pow(1.0 - dist_from_surface, 3.0) * 0.4;

    // 7. Metabolic pulsing
    float pulse = sin(u_time * 1.2 + seed * 6.28) * 0.1 + 0.9;

    // === COMBINE EFFECTS ===
    vec3 final_color = base_color;

    // Add subsurface scattering
    final_color = mix(final_color, scatter_color, scatter);

    // Darken for cytoplasm structure
    final_color *= (1.0 - cytoplasm * 0.6);

    // Add organelles (dark spots)
    final_color *= (1.0 - organelles);

    // Membrane texture variation
    final_color += vec3(membrane_noise * 0.1);

    // Edge glow
    final_color += vec3(edge_glow);

    // Metabolic pulse
    final_color *= pulse;

    // Compression stress visualization
    final_color += vec3(squish * 0.15);

    // Type-specific variations
    int type_id = int(type + 0.5);
    if (type_id == 0) {
        // Coccus: subtle shimmer
        float shimmer = sin(vTexCoord.x * 40.0 + u_time * 4.0) * 0.08;
        final_color += vec3(shimmer);
    } else if (type_id == 1) {
        // Bacillus: longitudinal structure
        float structure = abs(sin(vTexCoord.x * 25.0)) * 0.12;
        final_color *= (1.0 - structure);
    } else if (type_id == 2) {
        // Vibrio: curved flow pattern
        float flow = sin(vTexCoord.x * 15.0 + vTexCoord.y * 15.0 + u_time * 0.5) * 0.1;
        final_color += vec3(flow);
    } else if (type_id == 3) {
        // Spirillum: spiral pattern
        float spiral = sin(atan(vTexCoord.y - 0.5, vTexCoord.x - 0.5) * 3.0 - u_time) * 0.1;
        final_color += vec3(spiral);
    }

    // Final alpha (membrane transparency + edge antialiasing)
    float final_alpha = base_alpha * m.color.a * edge_alpha;

    // Ensure we don't go outside color range
    final_color = clamp(final_color, 0.0, 1.0);

    FragColor = vec4(final_color, final_alpha);
}
