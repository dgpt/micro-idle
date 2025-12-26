#version 460 core

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
out vec2 vDir;

void main() {
    uint idx = uint(gl_InstanceID);
    Entity e = entities[idx];

    float scale = e.params.x;
    float squish = e.params.y;
    float type = floor(e.params.z + 0.5);
    float seed = e.params.w;

    vec2 dir = normalize(e.vel.xz + vec2(0.0001));
    vec2 jitter = vec2(sin(seed * 12.3), cos(seed * 17.7)) * 0.25;
    dir = normalize(dir + jitter * 0.2);
    vDir = dir;

    float isB = 1.0 - step(0.5, abs(type - 1.0));
    float isV = 1.0 - step(0.5, abs(type - 2.0));
    float isS = 1.0 - step(0.5, abs(type - 3.0));
    float elongate = 1.0 + 0.8 * (isB + isV) + 1.2 * isS;
    float height = 1.0 - 0.1 * isS;

    float stretch = 1.0 + squish * 0.6;
    float squash = 1.0 - squish * 0.45;
    vec2 local = vec2(aPos.x * elongate * stretch, aPos.z * height * squash);

    mat2 rot = mat2(dir.x, -dir.y, dir.y, dir.x);
    vec2 rotated = rot * local;
    vec2 local01 = rotated * 2.0;

    float dome = max(0.0, 1.0 - dot(local01, local01));
    float wobble = sin(u_time * (1.4 + seed * 0.8) + dot(local01, vec2(4.0, 2.7))) * 0.05;
    vec3 world = e.pos.xyz + vec3(rotated.x, dome * (0.35 + 0.12 * squish) * (1.0 + wobble), rotated.y) * scale;

    gl_Position = u_vp * vec4(world, 1.0);

    vColor = e.color;
    vLocal = local01;
    vType = type;
    vSeed = seed;
    vSquish = squish;
}
