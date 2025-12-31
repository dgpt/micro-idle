#version 330

// SDF-based organic microbe rendering
in vec2 fragTexCoord;
in vec3 fragNormal;
in vec4 fragColor;
in vec3 fragWorldPos;

uniform vec3 viewPos;
uniform vec3 microbeColor;
uniform vec3 skeletonPoints[64];
uniform int pointCount;
uniform float baseRadius;
uniform float time;

out vec4 finalColor;

const int MAX_STEPS = 80;
const float MAX_DIST = 45.0;
const float SURF_DIST = 0.005;

float sdMembrane(vec3 p);

vec3 gCenter = vec3(0.0);
vec3 gBiasDir = vec3(1.0, 0.0, 0.0);
float gBiasStrength = 0.0;
vec3 gTipDir = vec3(1.0, 0.0, 0.0);
float gTipStrength = 0.0;
vec3 gSideDir = vec3(0.0, 0.0, 1.0);
float gSideStrength = 0.0;

// Smooth minimum for organic blending
float smin(float a, float b, float k) {
    float h = max(k - abs(a - b), 0.0) / k;
    return min(a, b) - h * h * h * k * (1.0 / 6.0);
}

float sdCapsule(vec3 p, vec3 a, vec3 b, float r) {
    vec3 pa = p - a;
    vec3 ba = b - a;
    float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
    return length(pa - ba * h) - r;
}

float hash3(vec3 p) {
    return fract(sin(dot(p, vec3(127.1, 311.7, 74.7))) * 43758.5453);
}

float noise3(vec3 p) {
    vec3 i = floor(p);
    vec3 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);

    float n000 = hash3(i + vec3(0.0, 0.0, 0.0));
    float n100 = hash3(i + vec3(1.0, 0.0, 0.0));
    float n010 = hash3(i + vec3(0.0, 1.0, 0.0));
    float n110 = hash3(i + vec3(1.0, 1.0, 0.0));
    float n001 = hash3(i + vec3(0.0, 0.0, 1.0));
    float n101 = hash3(i + vec3(1.0, 0.0, 1.0));
    float n011 = hash3(i + vec3(0.0, 1.0, 1.0));
    float n111 = hash3(i + vec3(1.0, 1.0, 1.0));

    float nx00 = mix(n000, n100, f.x);
    float nx10 = mix(n010, n110, f.x);
    float nx01 = mix(n001, n101, f.x);
    float nx11 = mix(n011, n111, f.x);
    float nxy0 = mix(nx00, nx10, f.y);
    float nxy1 = mix(nx01, nx11, f.y);
    return mix(nxy0, nxy1, f.z);
}

float fbm(vec3 p) {
    float v = 0.0;
    float a = 0.5;
    for (int i = 0; i < 3; i++) {
        v += a * noise3(p);
        p *= 2.0;
        a *= 0.5;
    }
    return v;
}

float softShadow(vec3 ro, vec3 rd, float maxDist) {
    float res = 1.0;
    float t = 0.02;
    for (int i = 0; i < 12; i++) {
        float h = sdMembrane(ro + rd * t);
        if (h < 0.001) {
            return 0.0;
        }
        res = min(res, 10.0 * h / t);
        t += clamp(h, 0.02, 0.2);
        if (t > maxDist) {
            break;
        }
    }
    return clamp(res, 0.0, 1.0);
}

// SDF for microbe membrane using skeleton points
float sdMembrane(vec3 p) {
    float d = 1e10;
    int count = pointCount;
    if (count > 64) {
        count = 64;
    }
    if (count <= 0) {
        return d;
    }

    float pointRadius = baseRadius * 0.65;
    float smoothK = pointRadius * 0.8;

    vec3 warp = vec3(
        fbm(p * 0.6 + vec3(1.7, 2.3, 3.1) + time * 0.12),
        fbm(p * 0.6 + vec3(4.2, 0.9, 2.0) + time * 0.1),
        fbm(p * 0.6 + vec3(2.8, 3.7, 1.1) + time * 0.08)
    );
    p += (warp - 0.5) * pointRadius * 0.2;

    for (int i = 0; i < 64; i++) {
        if (i >= count) {
            break;
        }
        float jitter = 0.88 + 0.18 * sin(dot(skeletonPoints[i], vec3(0.8, 1.3, 1.7)));
        float sphereDist = length(p - skeletonPoints[i]) - pointRadius * jitter;
        d = smin(d, sphereDist, smoothK);
    }

    vec3 q = p - gCenter;
    float along = dot(q, gBiasDir);
    vec3 radial = q - gBiasDir * along;
    float axialScale = 1.15;
    vec3 qScaled = radial + gBiasDir * (along / axialScale);
    float baseDist = length(qScaled) - baseRadius * 1.05;
    d = smin(d, baseDist, smoothK * 1.2);

    float podBlend = clamp(gBiasStrength / (baseRadius * 0.4), 0.2, 1.0);
    vec3 podStart = gCenter + gBiasDir * baseRadius * 0.15;
    vec3 podEnd = gCenter + gBiasDir * baseRadius * (0.7 + 0.6 * podBlend);
    float podRadius = baseRadius * mix(0.32, 0.48, podBlend);
    float podDist = sdCapsule(p, podStart, podEnd, podRadius);
    d = smin(d, podDist, smoothK * (1.05 + 0.35 * podBlend));

    float tipBlend = clamp(gTipStrength / (baseRadius * 0.9), 0.2, 1.0);
    vec3 tipStart = gCenter + gTipDir * baseRadius * 0.25;
    vec3 tipEnd = gCenter + gTipDir * baseRadius * (0.8 + 0.7 * tipBlend);
    float tipRadius = baseRadius * mix(0.28, 0.42, tipBlend);
    float tipDist = sdCapsule(p, tipStart, tipEnd, tipRadius);
    d = smin(d, tipDist, smoothK * (0.95 + 0.3 * tipBlend));

    float sideBlend = clamp(gSideStrength / (baseRadius * 0.6), 0.0, 1.0);
    vec3 sideStart = gCenter + gSideDir * baseRadius * 0.12;
    vec3 sideEnd = gCenter + gSideDir * baseRadius * (0.5 + 0.45 * sideBlend);
    float sideRadius = baseRadius * mix(0.2, 0.34, sideBlend);
    float sideDist = sdCapsule(p, sideStart, sideEnd, sideRadius);
    d = smin(d, sideDist, smoothK * (0.9 + 0.3 * sideBlend));

    float bump = (fbm(p * 1.4 + time * 0.15) - 0.5) * pointRadius * 0.22;
    return d + bump;
}

// Normal calculation
vec3 calcNormal(vec3 p) {
    float eps = max(0.003, baseRadius * 0.015);
    vec2 h = vec2(eps, 0.0);
    return normalize(vec3(
        sdMembrane(p + h.xyy) - sdMembrane(p - h.xyy),
        sdMembrane(p + h.yxy) - sdMembrane(p - h.yxy),
        sdMembrane(p + h.yyx) - sdMembrane(p - h.yyx)
    ));
}

void main()
{
    vec3 ro = viewPos;
    vec3 rd = normalize(fragWorldPos - viewPos);

    int count = pointCount > 64 ? 64 : pointCount;
    vec3 center = vec3(0.0);
    for (int i = 0; i < 64; i++) {
        if (i >= count) {
            break;
        }
        center += skeletonPoints[i];
    }
    center /= max(1.0, float(count));
    vec3 bias = vec3(0.0);
    float maxDist = 0.0;
    vec3 farDir = vec3(1.0, 0.0, 0.0);
    for (int i = 0; i < 64; i++) {
        if (i >= count) {
            break;
        }
        vec3 offset = skeletonPoints[i] - center;
        float dist = length(offset);
        if (dist > maxDist) {
            maxDist = dist;
            farDir = offset / dist;
        }
        bias += offset;
    }
    float biasLen = length(bias);
    vec3 fallbackDir = normalize(vec3(
        sin(dot(center, vec3(1.3, 2.1, 3.7))),
        sin(dot(center, vec3(2.9, 0.7, 1.1))),
        cos(dot(center, vec3(4.1, 0.7, 2.3)))
    ));
    gCenter = center;
    gBiasDir = biasLen > 0.001 ? bias / biasLen : fallbackDir;
    gBiasStrength = biasLen;
    gTipDir = maxDist > 0.001 ? farDir : gBiasDir;
    gTipStrength = maxDist;

    float sideDist = 0.0;
    vec3 sideDir = vec3(0.0, 0.0, 1.0);
    for (int i = 0; i < 64; i++) {
        if (i >= count) {
            break;
        }
        vec3 offset = skeletonPoints[i] - center;
        float along = dot(offset, gTipDir);
        vec3 orth = offset - gTipDir * along;
        float dist = length(orth);
        if (dist > sideDist) {
            sideDist = dist;
            sideDir = orth / dist;
        }
    }
    vec3 fallbackSide = normalize(cross(gTipDir, vec3(0.0, 1.0, 0.0)));
    if (length(fallbackSide) < 0.001) {
        fallbackSide = normalize(cross(gTipDir, vec3(1.0, 0.0, 0.0)));
    }
    gSideDir = sideDist > 0.001 ? sideDir : fallbackSide;
    gSideStrength = sideDist;

    float t = 0.0;
    float hit = -1.0;
    for (int i = 0; i < MAX_STEPS; i++) {
        vec3 p = ro + rd * t;
        float d = sdMembrane(p);
        if (d < SURF_DIST) {
            hit = t;
            break;
        }
        t += d;
        if (t > MAX_DIST) {
            break;
        }
    }

    if (hit < 0.0) {
        discard;
    }

    vec3 p = ro + rd * hit;
    vec3 n = calcNormal(p);
    vec3 lightDir = normalize(vec3(0.45, 0.85, 0.25));
    vec3 fillDir = normalize(vec3(-0.35, 0.65, -0.65));
    vec3 viewDir = normalize(viewPos - p);
    float wrap = 0.35;
    float shadow = pow(softShadow(p + n * 0.01, lightDir, 4.0), 1.35);
    float diff = clamp((dot(n, lightDir) + wrap) / (1.0 + wrap), 0.0, 1.0) * shadow;
    float diffFill = clamp((dot(n, fillDir) + 0.15) / 1.15, 0.0, 1.0);
    float spec = pow(max(dot(reflect(-lightDir, n), viewDir), 0.0), 20.0) * shadow;
    float specFill = pow(max(dot(reflect(-fillDir, n), viewDir), 0.0), 26.0) * 0.4;
    float rim = pow(1.0 - max(dot(n, viewDir), 0.0), 2.1);

    float occ = 0.0;
    for (int i = 1; i <= 4; i++) {
        float h = 0.04 * float(i);
        float d = sdMembrane(p + n * h);
        occ += max(h - d, 0.0);
    }
    float ao = clamp(1.0 - occ * 0.5, 0.35, 1.0);

    float cellNoise = fbm(p * 2.0 + vec3(0.0, 1.7, 3.1) + time * 0.05);
    float membrane = fbm(p * 6.0 + vec3(2.4, 0.3, 1.1) + time * 0.1);
    vec3 base = microbeColor * (0.8 + 0.2 * cellNoise);
    vec3 gelTint = vec3(0.85, 0.95, 0.9);
    float coreDepth = clamp(1.0 - length(p - center) / (baseRadius * 1.4), 0.0, 1.0);
    vec3 coreTint = mix(vec3(1.0, 0.85, 0.75), microbeColor, 0.65);
    vec3 subsurface = microbeColor * 0.55 * pow(max(dot(n, -lightDir), 0.0), 1.05);
    float fresnel = pow(1.0 - max(dot(n, viewDir), 0.0), 1.6);
    vec3 membraneTint = mix(base, base * 1.15, membrane * 0.6);
    float lighting = 0.25 + diff * 0.55 + diffFill * 0.2;
    vec3 color = mix(membraneTint, gelTint, 0.08 + 0.22 * fresnel) * lighting * ao;
    vec3 nucleusDir = normalize(vec3(
        hash3(center + vec3(1.3, 2.1, 3.7)) - 0.5,
        hash3(center + vec3(2.9, 0.7, 1.1)) - 0.5,
        hash3(center + vec3(4.1, 0.7, 2.3)) - 0.5
    ));
    vec3 nucleusPos = center + nucleusDir * baseRadius * 0.35;
    float nucleusDist = length(p - nucleusPos);
    float nucleus = smoothstep(baseRadius * 0.25, baseRadius * 0.1, nucleusDist);
    color = mix(color, vec3(0.95, 0.86, 0.8), nucleus * 0.3);
    color = mix(coreTint, color, 0.4 + 0.6 * (1.0 - coreDepth));
    color += subsurface * 0.55;
    color += vec3(0.95) * (spec * 0.32 + specFill * 0.18);
    color += vec3(0.18, 0.28, 0.22) * rim;

    finalColor = vec4(color, 1.0);
}
