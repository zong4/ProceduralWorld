#version 410 core

in vec3 teWorldPos;
in vec3 teNormal;
in vec3 teSphereDir;
in float teHeight;

out vec4 FragColor;

uniform vec3 lightDir;
uniform vec3 cameraPos;
uniform int renderMode;
uniform sampler2DArray proceduralWaterDepthTexture;
uniform sampler2DArray proceduralErosionMaskTexture;
uniform sampler2DArray proceduralBiomeWeightATexture;
uniform sampler2DArray proceduralBiomeWeightBTexture;
uniform sampler2DArray proceduralDomainWeightTexture;
uniform int terrainDebugOverlayMode;

#include "terrain_types.glsl"
#include "terrain_surface.glsl"
#include "terrain_lighting.glsl"
#include "terrain_debug.glsl"

void main()
{
    vec3 sphereDir = normalize(teSphereDir);
    vec3 shadingNormal = normalize(teNormal);
    SurfaceData surface = sampleSurfaceData(teHeight, teWorldPos, shadingNormal, sphereDir);

    if (terrainDebugOverlayMode == 1) {
        float mountainMask = clamp(sampleVec4ArraySeamless(proceduralDomainWeightTexture, sphereDir).x, 0.0, 1.0);
        float visibleMask = pow(mountainMask, 0.82);
        vec3 lowColor = vec3(0.42, 0.42, 0.40);
        vec3 midColor = vec3(0.72, 0.18, 0.16);
        vec3 highColor = vec3(1.0, 0.34, 0.18);
        vec3 color = mix(lowColor, midColor, smoothstep(0.08, 0.72, visibleMask));
        color = mix(color, highColor, smoothstep(0.68, 1.0, visibleMask));
        FragColor = vec4(color, 1.0);
        return;
    }

    vec4 debugOutput = debugSurfaceOutput(renderMode, surface, shadingNormal);
    if (debugOutput.r >= 0.0) {
        FragColor = debugOutput;
        return;
    }

    LightingData lighting = evaluateLighting(surface, teWorldPos, shadingNormal);
    FragColor = vec4(toneMapAndGamma(lighting.litColor), 1.0);
}
