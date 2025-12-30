#include "SDFShader.h"
#include <cstring>
#include <cstdio>

namespace micro_idle {
namespace rendering {

Shader loadSDFMembraneShader() {
    // Try paths relative to executable location
    Shader shader = LoadShader("../shaders/sdf_membrane.vert", "../shaders/sdf_membrane.frag");

    if (shader.id == 0) {
        // Try absolute path from project root
        shader = LoadShader("shaders/sdf_membrane.vert", "shaders/sdf_membrane.frag");
    }

    if (shader.id == 0) {
        printf("SDFShader: Failed to load shader from both paths\n");
    } else {
        printf("SDFShader: Loaded shader successfully\n");
    }

    return shader;
}

bool initializeSDFUniforms(Shader shader, SDFShaderUniforms& uniforms) {
    if (shader.id == 0) {
        return false;
    }

    // Initialize array to -1
    for (int i = 0; i < 64; i++) {
        uniforms.skeletonPoints[i] = -1;
    }

    // Get uniform locations
    uniforms.viewPos = GetShaderLocation(shader, "viewPos");
    uniforms.pointCount = GetShaderLocation(shader, "pointCount");
    uniforms.baseRadius = GetShaderLocation(shader, "baseRadius");
    uniforms.microbeColor = GetShaderLocation(shader, "microbeColor");

    // Get skeleton points array uniform locations
    for (int i = 0; i < 64; i++) {
        char uniformName[64];
        snprintf(uniformName, sizeof(uniformName), "skeletonPoints[%d]", i);
        uniforms.skeletonPoints[i] = GetShaderLocation(shader, uniformName);
    }

    // Check if critical uniforms were found
    bool valid = (uniforms.pointCount >= 0 && uniforms.baseRadius >= 0);

    if (!valid) {
        printf("SDFShader: Warning - some required uniforms not found\n");
    }

    return valid;
}

void setCameraPosition(Shader shader, const SDFShaderUniforms& uniforms, Vector3 cameraPos) {
    if (shader.id == 0 || uniforms.viewPos < 0) {
        return;
    }

    SetShaderValue(shader, uniforms.viewPos, &cameraPos, SHADER_UNIFORM_VEC3);
}

void setMicrobeUniforms(Shader shader, const SDFShaderUniforms& uniforms,
                        int vertexCount, float baseRadius, Color microbeColor) {
    if (shader.id == 0) {
        return;
    }

    if (uniforms.pointCount >= 0) {
        SetShaderValue(shader, uniforms.pointCount, &vertexCount, SHADER_UNIFORM_INT);
    }

    if (uniforms.baseRadius >= 0) {
        SetShaderValue(shader, uniforms.baseRadius, &baseRadius, SHADER_UNIFORM_FLOAT);
    }

    if (uniforms.microbeColor >= 0) {
        Vector3 colorVec = {
            microbeColor.r / 255.0f,
            microbeColor.g / 255.0f,
            microbeColor.b / 255.0f
        };
        SetShaderValue(shader, uniforms.microbeColor, &colorVec, SHADER_UNIFORM_VEC3);
    }
}

void setVertexPositions(Shader shader, const SDFShaderUniforms& uniforms,
                        const Vector3* positions, int count) {
    if (shader.id == 0 || positions == nullptr || count <= 0) {
        return;
    }

    // Upload vertex positions to shader uniforms
    for (int i = 0; i < count && i < 64; i++) {
        if (uniforms.skeletonPoints[i] >= 0) {
            float pos[3] = {positions[i].x, positions[i].y, positions[i].z};
            SetShaderValue(shader, uniforms.skeletonPoints[i], pos, SHADER_UNIFORM_VEC3);
        }
    }
}

} // namespace rendering
} // namespace micro_idle
