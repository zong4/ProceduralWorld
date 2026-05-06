#version 410 core

in vec3 teWorldPos;
in vec3 teNormal;
in vec2 teTexCoord;
in float teHeight;

out vec4 FragColor;

uniform vec3 lightDir;
uniform vec3 cameraPos;
uniform int renderMode;    // 0=shaded, 2=height map, 3=normals

#include "terrain_types.glsl"
#include "terrain_surface.glsl"
#include "terrain_lighting.glsl"
#include "terrain_debug.glsl"

void main()
{
    vec3 shadingNormal = normalize(teNormal);
    SurfaceData surface = sampleSurfaceData(teHeight, teWorldPos, shadingNormal);

    vec4 debugOutput = debugSurfaceOutput(renderMode, surface, shadingNormal);
    if (debugOutput.r >= 0.0) {
        FragColor = debugOutput;
        return;
    }

    LightingData lighting = evaluateLighting(surface, teWorldPos, shadingNormal);
    FragColor = vec4(toneMapAndGamma(lighting.litColor), 1.0);
}
