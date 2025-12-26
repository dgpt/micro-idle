#version 430 core

in vec4 vColor;
in vec2 vLocal;
in float vType;
in float vSquish;
in float vSeed;

uniform float u_time;

out vec4 fragColor;

float sdf_circle(vec2 p, float r) { return length(p) - r; }

float sdf_capsule(vec2 p, float halfLen, float radius) {
    vec2 q = vec2(abs(p.x) - halfLen, p.y);
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - radius;
}

float sdf_vibrio(vec2 p) {
    vec2 q = p;
    q.y -= p.x * p.x * 0.35;
    return length(q) - 0.95;
}

float sdf_spiral(vec2 p, float t, float seed) {
    float ang = atan(p.y, p.x);
    float r = length(p);
    float arm = 0.65 + 0.15 * sin(ang * 3.0 + t * 1.7 + seed * 3.0);
    return r - arm;
}

float sdf_amoeba(vec2 p, float t, float seed) {
    float ang = atan(p.y, p.x);
    float r = length(p);
    float wobble = sin(ang * 5.0 + t * 0.9 + seed * 4.0) * 0.18;
    float wobble2 = sin(ang * 9.0 - t * 1.3 + seed * 2.7) * 0.12;
    return r - (0.95 + wobble + wobble2);
}

float sdf_diatom(vec2 p) {
    vec2 q = abs(p);
    float d = max(q.x * 1.1 + q.y * 0.4, q.y * 1.4 + q.x * 0.4) - 0.95;
    return d;
}

float shape_sdf(int type, vec2 p, float t, float seed) {
    if (type == 0) { // Coccus
        return sdf_circle(p, 1.0);
    } else if (type == 1) { // Bacillus
        return sdf_capsule(p, 0.7, 0.55);
    } else if (type == 2) { // Vibrio
        return sdf_vibrio(p);
    } else if (type == 3) { // Spirillum
        return sdf_spiral(p, t, seed);
    } else if (type == 4) { // Amoeboid
        return sdf_amoeba(p, t, seed);
    } else { // Diatom / lens
        return sdf_diatom(p);
    }
}

vec3 shade_membrane(vec2 p, float sdf, vec3 base, float squish) {
    float rim = smoothstep(0.0, -0.12, sdf);
    float inner = smoothstep(-0.5, -0.1, sdf);
    vec3 color = mix(base * 0.6, base, inner);
    color = mix(color, base * 1.25, rim * 0.4);
    color = mix(color, vec3(1.0), squish * rim * 0.35);
    return color;
}

void main() {
    vec2 p = vLocal;
    int type = int(floor(vType + 0.5));
    float t = u_time;

    float sdf = shape_sdf(type, p, t, vSeed);
    float alpha = 1.0 - smoothstep(0.0, 0.08, sdf);
    if (alpha < 0.01) discard;

    // Approx normal from sdf gradient
    float eps = 0.003;
    float dx = shape_sdf(type, p + vec2(eps, 0.0), t, vSeed) - sdf;
    float dy = shape_sdf(type, p + vec2(0.0, eps), t, vSeed) - sdf;
    vec3 normal = normalize(vec3(-dx, -dy, 1.0));

    vec3 lightDir = normalize(vec3(0.25, 0.7, 0.6));
    float lit = clamp(dot(normal, lightDir), 0.0, 1.0);

    vec3 base = vColor.rgb;
    vec3 color = shade_membrane(p, sdf, base, vSquish);

    // Core glow
    float core = smoothstep(-0.35, -0.08, sdf);
    color = mix(color, base * 1.35, core * 0.5);

    // Subtle axial striations for rods/diatoms
    if (type == 1 || type == 5) {
        float bands = sin((p.x + p.y * 0.3) * 12.0 + vSeed * 5.0);
        bands = bands * 0.5 + 0.5;
        color = mix(color, color * 0.8, bands * 0.12);
    }

    // Spiral highlight
    if (type == 3) {
        float ang = atan(p.y, p.x);
        float highlight = sin(ang * 3.0 + t * 2.0 + vSeed * 6.0) * 0.5 + 0.5;
        color = mix(color, base * 1.5, highlight * 0.25);
    }

    // Final lighting
    color *= mix(0.9, 1.25, lit);

    fragColor = vec4(color, alpha * vColor.a);
}
