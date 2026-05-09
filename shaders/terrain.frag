#version 410 core

in vec3 teWorldPos;
in vec3 teNormal;
in vec3 teSphereDir;
in float teHeight;
in float teSurfaceHeight;
in float teSkirt;

out vec4 FragColor;

uniform vec3 lightDir;
uniform vec3 cameraPos;
uniform int renderMode;    // 0=shaded, 2=height map, 3=normals
uniform int terrainMaskDebugMode;
uniform sampler2DArray proceduralWaterDepthTexture;
uniform sampler2DArray proceduralErosionMaskTexture;
uniform sampler2DArray proceduralBiomeWeightATexture;
uniform sampler2DArray proceduralBiomeWeightBTexture;

#include "terrain_types.glsl"
#include "terrain_surface.glsl"
#include "terrain_lighting.glsl"
#include "terrain_debug.glsl"

void main()
{
    float skirtMask = clamp(teSkirt, 0.0, 1.0);
    vec3 sphereDir = normalize(teSphereDir);
    vec3 shadingNormal = normalize(mix(normalize(teNormal), sphereDir, skirtMask));
    float surfaceHeight = mix(teHeight, teSurfaceHeight, skirtMask);
    SurfaceData surface = sampleSurfaceData(surfaceHeight, teWorldPos, shadingNormal, sphereDir);

    if (terrainMaskDebugMode > 0) {
        vec3 sampleDir = sphereDir;
        PlanetSample planet = samplePlanet(sampleDir, surfaceHeight);
        vec4 erosionData = planet.erosionData;
        float value = 0.0;
        if (terrainMaskDebugMode == 1) value = erosionData.r;
        if (terrainMaskDebugMode == 2) value = erosionData.g;
        if (terrainMaskDebugMode == 3) value = erosionData.b;
        if (terrainMaskDebugMode == 4) value = erosionData.a;
        if (terrainMaskDebugMode == 5) value = planet.shoreMask;
        if (terrainMaskDebugMode == 6) value = clamp(planet.waterDepth * 0.20, 0.0, 1.0);
        if (terrainMaskDebugMode == 7) value = planet.temperature;
        if (terrainMaskDebugMode == 8) value = planet.moisture;
        if (terrainMaskDebugMode == 9) value = smoothstep(0.0, 0.08, surfaceHeight);
        if (terrainMaskDebugMode == 10) {
            vec4 biomeA = planet.biomeA;
            vec4 biomeB = planet.biomeB;
            vec3 beachColor = vec3(1.00, 0.78, 0.30);
            vec3 grassColor = vec3(0.18, 0.90, 0.20);
            vec3 forestColor = vec3(0.00, 0.34, 0.08);
            vec3 desertColor = vec3(1.00, 0.48, 0.05);
            vec3 rockColor = vec3(0.47, 0.47, 0.50);
            vec3 snowColor = vec3(0.88, 0.97, 1.00);
            vec3 wetlandColor = vec3(0.00, 0.66, 0.58);
            vec3 shallowWaterColor = vec3(0.00, 0.36, 1.00);
            vec3 biomeColor =
                beachColor * biomeA.r +
                grassColor * biomeA.g +
                forestColor * biomeA.b +
                desertColor * biomeA.a +
                rockColor * biomeB.r +
                snowColor * biomeB.g +
                wetlandColor * biomeB.b +
                shallowWaterColor * biomeB.a;
            float weightSum = dot(biomeA, vec4(1.0)) + dot(biomeB, vec4(1.0));
            FragColor = vec4(biomeColor / max(weightSum, 0.0001), 1.0);
            return;
        }
        if (terrainMaskDebugMode == 11) {
            vec4 erosion = planet.erosionData;
            FragColor = vec4(erosion.r, erosion.g, erosion.a, 1.0);
            return;
        }
        vec3 redWhite = mix(vec3(0.10, 0.0, 0.0), vec3(1.0, 0.96, 0.92), clamp(value, 0.0, 1.0));
        FragColor = vec4(redWhite, 1.0);
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
