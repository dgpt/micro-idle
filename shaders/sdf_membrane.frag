#version 430

// SDF Raymarching fragment shader
// Renders smooth organic membrane over point cloud skeleton

in vec2 fragTexCoord;
in vec3 fragNormal;
in vec4 fragColor;
in vec3 fragWorldPos;            // World position from vertex shader

uniform vec3 viewPos;            // Camera position
uniform vec3 skeletonPoints[64]; // Skeleton vertex positions (world space)
uniform int pointCount;          // Number of active points
uniform float baseRadius;        // Base radius for each skeleton sphere
uniform vec3 microbeColor;       // Microbe color

out vec4 finalColor;

// Smooth minimum for organic blending
float smin(float a, float b, float k) {
    float h = max(k - abs(a - b), 0.0) / k;
    return min(a, b) - h * h * h * k * (1.0 / 6.0);
}

// SDF for the microbe membrane
float sdMembrane(vec3 p) {
    float d = 1e10;
    float smoothness = 0.5; // Higher = more blobby

    for (int i = 0; i < pointCount && i < 64; i++) {
        float sphereDist = length(p - skeletonPoints[i]) - baseRadius;
        d = smin(d, sphereDist, smoothness);
    }

    return d;
}

// Calculate normal using gradient
vec3 calcNormal(vec3 p) {
    const float eps = 0.001;
    vec2 h = vec2(eps, 0.0);
    return normalize(vec3(
        sdMembrane(p + h.xyy) - sdMembrane(p - h.xyy),
        sdMembrane(p + h.yxy) - sdMembrane(p - h.yxy),
        sdMembrane(p + h.yyx) - sdMembrane(p - h.yyx)
    ));
}

// Raymarch through the SDF
float raymarch(vec3 ro, vec3 rd) {
    float t = 0.0;
    const int maxSteps = 64;
    const float maxDist = 100.0;
    const float surfDist = 0.01;

    for (int i = 0; i < maxSteps; i++) {
        vec3 p = ro + rd * t;
        float d = sdMembrane(p);

        if (d < surfDist) {
            return t;
        }

        if (t > maxDist) {
            break;
        }

        t += d;
    }

    return -1.0;
}

void main()
{
    // Proper ray calculation: from camera through fragment's world position
    vec3 rayOrigin = viewPos;
    vec3 rayDir = normalize(fragWorldPos - viewPos);

    // Raymarch from the bounding surface inward
    float t = raymarch(rayOrigin, rayDir);

    if (t < 0.0) {
        // Missed the membrane
        discard;
    }

    // Hit point
    vec3 p = rayOrigin + rayDir * t;
    vec3 normal = calcNormal(p);

    // Simple lighting
    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.3));
    float diff = max(dot(normal, lightDir), 0.0);
    float ambient = 0.3;
    float light = ambient + diff * 0.7;

    // Fresnel-like rim lighting for organic look
    float fresnel = pow(1.0 - max(dot(normal, -rayDir), 0.0), 3.0);

    vec3 col = microbeColor * light + vec3(0.2) * fresnel;

    finalColor = vec4(col, 0.85); // Slightly transparent
}
