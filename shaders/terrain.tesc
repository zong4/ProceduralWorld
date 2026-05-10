#version 410 core

// Input patch: 4 control points (quad patch)
layout(vertices = 4) out;

in vec2 vTexCoord[];
in float vSkirt[];
out vec2 tcTexCoord[];
out float tcSkirt[];

uniform vec3 cameraPos;
uniform mat4 model;
uniform vec3 faceNormal;
uniform vec3 faceAxisU;
uniform vec3 faceAxisV;
uniform float planetRadius;
uniform float tessMin;       // Minimum tessellation level (far)
uniform float tessMax;       // Maximum tessellation level (near)
uniform float tessMinDist;   // Distance at which max tess starts
uniform float tessMaxDist;   // Distance beyond which min tess is used
uniform float heightScale;
uniform float seaLevelOffset;
uniform float oceanShoreBlendWidth;
uniform float proceduralDataTexelSize;
uniform int useProceduralData;
uniform sampler2DArray proceduralHeightTexture;
uniform sampler2DArray proceduralWaterDepthTexture;
uniform sampler2DArray proceduralErosionMaskTexture;
uniform sampler2DArray proceduralBiomeWeightATexture;
uniform sampler2DArray proceduralBiomeWeightBTexture;

#include "planet_sampling.glsl"

vec3 cubeSphereDirection(vec2 uv)
{
    vec2 faceUV = uv * 2.0 - 1.0;
    vec3 cubePos = faceNormal + faceUV.x * faceAxisU + faceUV.y * faceAxisV;
    return normalize(cubePos);
}

vec3 cubeSpherePosition(vec2 uv)
{
    return cubeSphereDirection(uv) * planetRadius;
}

float cubeSphereLodScale(vec3 sphereDir)
{
    vec3 a = abs(normalize(sphereDir));
    return clamp(1.0 / max(max(a.x, a.y), max(a.z, 0.0001)), 1.0, 1.75);
}

float sampleBakedHeight(vec2 uv)
{
    return sampleFloatArraySeamlessNarrow(proceduralHeightTexture, cubeSphereDirection(uv));
}

float computeTerrainComplexity(vec2 uv, float uvRadius)
{
    if (useProceduralData == 0 || proceduralDataTexelSize <= 0.0) {
        return 0.0;
    }

    vec2 eps = vec2(max(max(uvRadius * 0.65, proceduralDataTexelSize * 2.0), 0.0015), 0.0);
    vec2 uvL = clamp(uv - eps.xy, vec2(0.0), vec2(1.0));
    vec2 uvR = clamp(uv + eps.xy, vec2(0.0), vec2(1.0));
    vec2 uvD = clamp(uv - eps.yx, vec2(0.0), vec2(1.0));
    vec2 uvU = clamp(uv + eps.yx, vec2(0.0), vec2(1.0));

    float hC = sampleBakedHeight(uv);
    float hL = sampleBakedHeight(uvL);
    float hR = sampleBakedHeight(uvR);
    float hD = sampleBakedHeight(uvD);
    float hU = sampleBakedHeight(uvU);
    float heightRange = (max(max(hL, hR), max(hD, hU)) - min(min(hL, hR), min(hD, hU))) * heightScale;
    float curvature = abs((hL + hR + hD + hU) * 0.25 - hC) * heightScale;
    float relief = clamp(smoothstep(0.25, 2.2, heightRange) + smoothstep(0.08, 0.75, curvature), 0.0, 1.0);

    vec3 dir = cubeSphereDirection(uv);
    float waterDepth = max(sampleFloatArraySeamless(proceduralWaterDepthTexture, dir), 0.0);
    float shore = 1.0 - smoothstep(0.0, max(oceanShoreBlendWidth * 1.4, 0.001), abs((seaLevelOffset - hC) * heightScale));
    shore = max(shore, 1.0 - smoothstep(0.0, max(oceanShoreBlendWidth * 2.0, 0.002), waterDepth));

    vec4 erosion = clamp(sampleVec4ArraySeamless(proceduralErosionMaskTexture, dir), vec4(0.0), vec4(1.0));
    float erosionDetail = clamp(erosion.r * 0.45 + erosion.g * 0.35 + erosion.b * 0.65 + erosion.a * 0.25, 0.0, 1.0);

    vec4 biomeA = clamp(sampleVec4ArraySeamless(proceduralBiomeWeightATexture, dir), vec4(0.0), vec4(1.0));
    vec4 biomeB = clamp(sampleVec4ArraySeamless(proceduralBiomeWeightBTexture, dir), vec4(0.0), vec4(1.0));
    float mountainMaterial = clamp(biomeB.r * 0.65 + biomeB.g * 0.45, 0.0, 1.0);
    float softPlain = clamp(biomeA.g * 0.30 + biomeA.b * 0.22 + biomeB.b * 0.55 + biomeA.r * 0.45, 0.0, 0.75);

    float complexity = relief * 0.55 + shore * 0.32 + erosionDetail * 0.34 + mountainMaterial * relief * 0.42;
    complexity *= 1.0 - softPlain * 0.35;
    return clamp(complexity, 0.0, 1.0);
}

// Compute tessellation level based on distance from camera
float computeTessLevel(vec2 uv, float uvRadius)
{
    vec3 sphereDir = cubeSphereDirection(uv);
    vec4 worldPos = model * vec4(sphereDir * planetRadius, 1.0);
    float sampleDist = length(cameraPos - worldPos.xyz);
    float lodScale = cubeSphereLodScale(sphereDir);
    float patchWorldRadius = planetRadius * uvRadius * 2.35 * lodScale;
    float dist = max((sampleDist - patchWorldRadius) / lodScale, 0.001);
    
    // Linearly interpolate based on distance
    float t = clamp((dist - tessMinDist) / (tessMaxDist - tessMinDist), 0.0, 1.0);
    float distanceLevel = mix(tessMax, tessMin, t);
    float complexity = computeTerrainComplexity(uv, uvRadius);
    float complexityLevel = mix(distanceLevel, tessMax, complexity * (1.0 - t * 0.45));
    return clamp(complexityLevel, tessMin, tessMax);
}

void main()
{
    // Pass-through control point data
    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
    tcTexCoord[gl_InvocationID] = vTexCoord[gl_InvocationID];
    tcSkirt[gl_InvocationID] = vSkirt[gl_InvocationID];

    // Only compute tessellation levels once (invocation 0)
    if (gl_InvocationID == 0)
    {
        // Compute levels at edge midpoints for smooth transitions
        // Edge 0: bottom (v=0), vertices 0 and 1
        vec2 mid0 = (vTexCoord[0] + vTexCoord[1]) * 0.5;
        // Edge 1: right  (u=1), vertices 1 and 2
        vec2 mid1 = (vTexCoord[1] + vTexCoord[2]) * 0.5;
        // Edge 2: top    (v=1), vertices 2 and 3
        vec2 mid2 = (vTexCoord[2] + vTexCoord[3]) * 0.5;
        // Edge 3: left   (u=0), vertices 3 and 0
        vec2 mid3 = (vTexCoord[3] + vTexCoord[0]) * 0.5;
        // Interior: patch center
        vec2 center = (vTexCoord[0] + vTexCoord[1] + vTexCoord[2] + vTexCoord[3]) * 0.25;

        float edgeRadius0 = length(vTexCoord[3] - vTexCoord[0]) * 0.5;
        float edgeRadius1 = length(vTexCoord[0] - vTexCoord[1]) * 0.5;
        float edgeRadius2 = length(vTexCoord[1] - vTexCoord[2]) * 0.5;
        float edgeRadius3 = length(vTexCoord[2] - vTexCoord[3]) * 0.5;
        float patchRadius = max(max(edgeRadius0, edgeRadius1), max(edgeRadius2, edgeRadius3));

        gl_TessLevelOuter[0] = computeTessLevel(mid3, edgeRadius0);
        gl_TessLevelOuter[1] = computeTessLevel(mid0, edgeRadius1);
        gl_TessLevelOuter[2] = computeTessLevel(mid1, edgeRadius2);
        gl_TessLevelOuter[3] = computeTessLevel(mid2, edgeRadius3);

        gl_TessLevelInner[0] = computeTessLevel(center, patchRadius);
        gl_TessLevelInner[1] = computeTessLevel(center, patchRadius);

    }
}
