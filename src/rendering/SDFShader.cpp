#include "SDFShader.h"
#include <string>

namespace micro_idle {
namespace rendering {

namespace {

bool tryLoadShader(Shader& shader, const char* vertPath, const char* fragPath) {
    if (shader.id != 0) {
        return true;
    }
    if (FileExists(vertPath) && FileExists(fragPath)) {
        shader = LoadShader(vertPath, fragPath);
    }
    return shader.id != 0;
}

std::string joinPath(const char* base, const char* suffix) {
    std::string path = base ? base : "";
    if (!path.empty()) {
        char back = path.back();
        if (back != '/' && back != '\\') {
            path.push_back('/');
        }
    }
    path += suffix;
    return path;
}

} // namespace

Shader loadSDFMembraneShader() {
    // Try multiple paths to find the shader files
    const char* paths[][2] = {
        {"../shaders/sdf_membrane.vert", "../shaders/sdf_membrane.frag"},
        {"shaders/sdf_membrane.vert", "shaders/sdf_membrane.frag"},
        {"../data/shaders/sdf_membrane.vert", "../data/shaders/sdf_membrane.frag"},
        {"data/shaders/sdf_membrane.vert", "data/shaders/sdf_membrane.frag"}
    };

    Shader shader = {0};

    for (int i = 0; i < 4; i++) {
        if (tryLoadShader(shader, paths[i][0], paths[i][1])) {
            break;
        }
    }

    if (shader.id == 0) {
        const char* appDir = GetApplicationDirectory();
        if (appDir && appDir[0] != '\0') {
            std::string appShadersVert = joinPath(appDir, "shaders/sdf_membrane.vert");
            std::string appShadersFrag = joinPath(appDir, "shaders/sdf_membrane.frag");
            std::string appDataVert = joinPath(appDir, "data/shaders/sdf_membrane.vert");
            std::string appDataFrag = joinPath(appDir, "data/shaders/sdf_membrane.frag");
            std::string parentShadersVert = joinPath(appDir, "../shaders/sdf_membrane.vert");
            std::string parentShadersFrag = joinPath(appDir, "../shaders/sdf_membrane.frag");
            std::string parentDataVert = joinPath(appDir, "../data/shaders/sdf_membrane.vert");
            std::string parentDataFrag = joinPath(appDir, "../data/shaders/sdf_membrane.frag");

            tryLoadShader(shader, appShadersVert.c_str(), appShadersFrag.c_str());
            tryLoadShader(shader, appDataVert.c_str(), appDataFrag.c_str());
            tryLoadShader(shader, parentShadersVert.c_str(), parentShadersFrag.c_str());
            tryLoadShader(shader, parentDataVert.c_str(), parentDataFrag.c_str());
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
    uniforms.podDirs = GetShaderLocation(shader, "podDirs[0]");
    uniforms.podExtents = GetShaderLocation(shader, "podExtents[0]");
    uniforms.podAnchors = GetShaderLocation(shader, "podAnchors[0]");
    uniforms.podCount = GetShaderLocation(shader, "podCount");

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

void setPodData(Shader shader, const SDFShaderUniforms& uniforms,
                const Vector3* podDirs, const float* podExtents,
                const Vector3* podAnchors, int podCount) {
    if (shader.id == 0) {
        return;
    }

    int clampedCount = podCount < 0 ? 0 : podCount;
    if (clampedCount > 4) {
        clampedCount = 4;
    }

    if (uniforms.podCount >= 0) {
        SetShaderValue(shader, uniforms.podCount, &clampedCount, SHADER_UNIFORM_INT);
    }

    if (clampedCount <= 0) {
        return;
    }

    if (uniforms.podDirs >= 0 && podDirs) {
        float values[4 * 3];
        for (int i = 0; i < clampedCount; i++) {
            int base = i * 3;
            values[base + 0] = podDirs[i].x;
            values[base + 1] = podDirs[i].y;
            values[base + 2] = podDirs[i].z;
        }
        SetShaderValueV(shader, uniforms.podDirs, values, SHADER_UNIFORM_VEC3, clampedCount);
    }

    if (uniforms.podExtents >= 0 && podExtents) {
        float values[4];
        for (int i = 0; i < clampedCount; i++) {
            values[i] = podExtents[i];
        }
        SetShaderValueV(shader, uniforms.podExtents, values, SHADER_UNIFORM_FLOAT, clampedCount);
    }

    if (uniforms.podAnchors >= 0 && podAnchors) {
        float values[4 * 3];
        for (int i = 0; i < clampedCount; i++) {
            int base = i * 3;
            values[base + 0] = podAnchors[i].x;
            values[base + 1] = podAnchors[i].y;
            values[base + 2] = podAnchors[i].z;
        }
        SetShaderValueV(shader, uniforms.podAnchors, values, SHADER_UNIFORM_VEC3, clampedCount);
    }
}

} // namespace rendering
} // namespace micro_idle
