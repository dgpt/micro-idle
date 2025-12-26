#version 430 core

layout(location = 0) in vec3 aPos;

struct Entity { vec4 pos; vec4 vel; vec4 color; vec4 params; };
layout(std430, binding = 0) readonly buffer Entities { Entity entities[]; };

layout(location = 0) uniform mat4 u_vp;
uniform float u_time;

out vec4 vColor;
out vec2 vLocal;
out float vType;
out float vSeed;
out float vSquish;
float hash11(float p) { return fract(sin(p * 127.1) * 43758.5453); }

mat2 rotate2(float a) { return mat2(cos(a), -sin(a), sin(a), cos(a)); }

vec2 bend_vibrio(vec2 p, float t) {
    float curve = 0.35 + sin(t * 1.7) * 0.08;
    p.y += p.x * p.x * curve;
    return p;
}

vec2 twist_spiral(vec2 p, float t) {
    float wave = sin(p.x * 6.0 + t * 2.5) * 0.25;
    p.y += wave;
    float roll = sin(t * 1.3 + p.x * 4.0) * 0.35;
    p = rotate2(roll) * p;
    return p;
}

vec2 amoeba_warp(vec2 p, float t, float seed) {
    float ang = atan(p.y, p.x);
    float wobble = sin(ang * 5.0 + t * 1.2 + seed * 6.28) * 0.18;
    float wobble2 = sin(ang * 9.0 - t * 0.9 + seed * 4.12) * 0.08;
    float r = length(p);
    r *= 1.0 + wobble + wobble2;
    return vec2(cos(ang), sin(ang)) * r;
}

vec2 diatom_warp(vec2 p, float t) {
    p.x *= 1.2;
    p.y *= 0.85 + sin(t * 1.8) * 0.04;
    return p;
}

void main() {
    uint idx = uint(gl_InstanceID);
    Entity e = entities[idx];

    float scale = e.params.x;
    float squish = clamp(e.params.y, 0.0, 1.2);
    float type = floor(e.params.z + 0.5);
    float seed = e.params.w;
    float t = u_time;

    float speed = length(e.vel.xz);
    vec2 dir = speed > 0.001 ? normalize(e.vel.xz) : vec2(cos(seed * 6.28), sin(seed * 6.28));

    // Normalize local quad to [-1, 1]
    const float baseSpan = 1.4;
    vec2 local = aPos.xz / baseSpan;

    // Direction-aligned squish
    mat2 rot = rotate2(atan(dir.y, dir.x));
    mat2 rotInv = rotate2(-atan(dir.y, dir.x));
    vec2 velSpace = rotInv * local;
    velSpace.x *= (1.0 - squish * 0.35);
    velSpace.y *= (1.0 + squish * 0.35);
    local = rot * velSpace;

    // Subtle breathing
    float breathe = 1.0 + sin(t * (0.8 + seed * 0.4) + seed * 6.28) * 0.05;
    local *= breathe;

    int itype = int(type);
    if (itype == 1) { // Bacillus capsule
        local.x *= 1.5;
        local.y *= 0.65;
    } else if (itype == 2) { // Vibrio comma
        local.x *= 1.2;
        local.y *= 0.7;
        local = bend_vibrio(local, t + seed * 2.0);
    } else if (itype == 3) { // Spirillum helix
        local.x *= 1.1;
        local.y *= 0.75;
        local = twist_spiral(local, t + seed * 3.0);
    } else if (itype == 4) { // Amoeboid
        local = amoeba_warp(local, t, seed);
    } else if (itype == 5) { // Diatom / lens
        local = diatom_warp(local, t);
    }

    // Swim drift
    vec2 swim = dir * sin(t * (1.5 + seed * 0.3)) * 0.08;
    local += swim;

    // Dome height for parallax
    float radial = length(local);
    float dome = sqrt(max(0.0, 1.0 - radial * radial));
    float domeHeight = mix(0.22, 0.48, clamp(1.0 - radial, 0.0, 1.0));
    domeHeight += squish * 0.2;

    vec3 world = e.pos.xyz;
    world.xz += local * scale;
    world.y += dome * domeHeight * scale;

    gl_Position = u_vp * vec4(world, 1.0);

    vColor = e.color;
    vLocal = local;
    vType = type;
    vSeed = seed;
    vSquish = squish;
}
