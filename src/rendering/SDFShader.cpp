#include "SDFShader.h"
namespace micro_idle {
namespace rendering {

Shader loadSDFMembraneShader() {
    // Try multiple paths to find the shader files
    const char* paths[][2] = {
        {"../shaders/sdf_membrane.vert", "../shaders/sdf_membrane.frag"},
        {"shaders/sdf_membrane.vert", "shaders/sdf_membrane.frag"},
        {"../data/shaders/sdf_membrane.vert", "../data/shaders/sdf_membrane.frag"},
        {"data/shaders/sdf_membrane.vert", "data/shaders/sdf_membrane.frag"}
    };

    Shader shader = {0};

    for (int i = 0; i < 4 && shader.id == 0; i++) {
        if (FileExists(paths[i][0]) && FileExists(paths[i][1])) {
            shader = LoadShader(paths[i][0], paths[i][1]);
        }
    }

    return shader;
}

bool initializeSDFUniforms(Shader shader, SDFShaderUniforms& uniforms) {
    if (shader.id == 0) {
        return false;
    }

    // Get uniform locations
    uniforms.viewPos = GetShaderLocation(shader, "viewPos");
    uniforms.time = GetShaderLocation(shader, "time");
    uniforms.pointCount = GetShaderLocation(shader, "pointCount");
    uniforms.baseRadius = GetShaderLocation(shader, "baseRadius");
    uniforms.microbeColor = GetShaderLocation(shader, "microbeColor");
    uniforms.skeletonPoints = GetShaderLocation(shader, "skeletonPoints[0]");

    // Check that critical uniforms were found
    return uniforms.viewPos >= 0 &&
           uniforms.pointCount >= 0 &&
           uniforms.baseRadius >= 0 &&
           uniforms.microbeColor >= 0 &&
           uniforms.skeletonPoints >= 0;
}

void setCameraPosition(Shader shader, const SDFShaderUniforms& uniforms, Vector3 cameraPos) {
    if (shader.id == 0 || uniforms.viewPos < 0) {
        return;
    }

    SetShaderValue(shader, uniforms.viewPos, &cameraPos, SHADER_UNIFORM_VEC3);
}

void setTime(Shader shader, const SDFShaderUniforms& uniforms, float time) {
    if (shader.id == 0 || uniforms.time < 0) {
        return;
    }

    SetShaderValue(shader, uniforms.time, &time, SHADER_UNIFORM_FLOAT);
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
    if (shader.id == 0 || positions == nullptr || count <= 0 || uniforms.skeletonPoints < 0) {
        return;
    }

    int clampedCount = count > 64 ? 64 : count;
    float values[64 * 3];
    for (int i = 0; i < clampedCount; i++) {
        int base = i * 3;
        values[base + 0] = positions[i].x;
        values[base + 1] = positions[i].y;
        values[base + 2] = positions[i].z;
    }

    SetShaderValueV(shader, uniforms.skeletonPoints, values, SHADER_UNIFORM_VEC3, clampedCount);
}

} // namespace rendering
} // namespace micro_idle
