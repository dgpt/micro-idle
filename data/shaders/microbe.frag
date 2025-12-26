#version 460 core

in vec4 vColor;
in vec2 vLocal;
in float vType;
in float vSeed;
in float vSquish;
in vec2 vDir;

uniform float u_time;

out vec4 fragColor;

const float TAU = 6.2831853;

float hash21(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7)) + vSeed * 13.7) * 43758.5453);
}

float sdCircle(vec2 p, float r) {
    return length(p) - r;
}

float sdCapsule(vec2 p, vec2 a, vec2 b, float r) {
    vec2 pa = p - a, ba = b - a;
    float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
    return length(pa - ba * h) - r;
}

float spikeField(vec2 p, int count, float base, float amp) {
    float ang = atan(p.y, p.x);
    float spike = sin(ang * float(count) + vSeed * 11.7) * amp;
    return length(p) - (base + spike);
}

float bandNoise(vec2 p, float freq) {
    vec2 g = floor(p * freq);
    float n = hash21(g);
    return smoothstep(0.25, 0.0, length(fract(p * freq) - 0.5)) * n;
}

void main() {
    vec2 p = vLocal;
    float seed = vSeed * TAU;

    float isC = 1.0 - step(0.5, abs(vType - 0.0));
    float isB = 1.0 - step(0.5, abs(vType - 1.0));
    float isV = 1.0 - step(0.5, abs(vType - 2.0));
    float isS = 1.0 - step(0.5, abs(vType - 3.0));

    float wobble = sin(u_time * (1.2 + vSquish) + seed * 1.3) * 0.08;

    float sdfC = spikeField(p * 0.95, 12, 0.72 + wobble * 0.35, 0.18);
    float sdfB = sdCapsule(p + vec2(0.0, wobble * 0.4), vec2(-1.4, 0.0), vec2(1.4, 0.0), 0.45);
    float sdfV = sdCapsule(p + vec2(0.0, sin(p.x * 2.5 + seed) * 0.55), vec2(-1.2, 0.0), vec2(1.2, 0.0), 0.36);
    float sdfS = sdCapsule(p + vec2(0.0, sin(p.x * 4.5 + seed) * 0.65), vec2(-1.3, 0.0), vec2(1.3, 0.0), 0.30);

    float sdf = sdfC * isC + sdfB * isB + sdfV * isV + sdfS * isS;

    float body = smoothstep(0.12, -0.12, sdf);
    float outline = smoothstep(0.16, -0.16, sdf);
    float rim = smoothstep(0.04, -0.02, sdf + 0.1);
    float membrane = smoothstep(0.06, 0.0, abs(sdf + 0.05));

    float nucleus = smoothstep(0.32, 0.0, sdCircle(p * 0.85 + vec2(0.1 * sin(seed), 0.1 * cos(seed)), 0.26));
    float vacuole = smoothstep(0.14, 0.0, abs(sdCircle(p + vec2(0.3 * sin(seed * 0.7), 0.3 * cos(seed * 0.8)), 0.32)));
    float dots = bandNoise(p + seed, 6.0);

    float cilia = smoothstep(0.2, 0.0, abs(sin(atan(p.y, p.x) * 10.0 + seed))) *
                  smoothstep(0.04, 0.0, abs(length(p) - 0.95)) * isC;

    float flagella = 0.0;
    if (isB + isV > 0.0) {
        vec2 tailBase = vec2(sign(vDir.x) * 1.2, 0.0);
        vec2 tailDir = normalize(vec2(vDir.x, vDir.y + 0.001));
        float t = clamp(dot(p - tailBase, tailDir), -0.4, 1.6);
        vec2 closest = tailBase + tailDir * t;
        float wave = sin(t * 6.0 + u_time * 2.4 + seed) * 0.28;
        closest.y += wave;
        float dist = length(p - closest);
        flagella = smoothstep(0.06, 0.0, dist) * (isB + isV);
    }

    float spines = smoothstep(0.08, 0.0, abs(p.x) - 1.05) * smoothstep(0.4, 0.0, abs(p.y)) * isS;

    float alpha = max(body, max(flagella, max(cilia, spines)));
    if (alpha <= 0.001) discard;

    vec3 base = vColor.rgb;
    vec3 fill = mix(base, vec3(1.0), 0.18);
    vec3 color = mix(base * 0.4, fill, outline);
    color = mix(color, vec3(0.98, 0.78, 0.9), nucleus * 0.7);
    color = mix(color, vec3(0.9, 0.97, 1.0), vacuole * 0.45);
    color = mix(color, base * 0.6 + vec3(0.05, 0.07, 0.08), dots * 0.55);
    color = mix(color, base * 0.5, membrane * 0.35);
    color = mix(color, vec3(1.0), rim * 0.12);

    fragColor = vec4(color, alpha);
}
