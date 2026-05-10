#version 410 core

in vec2 teTexCoord;
in vec3 teSphereDir;

out vec4 FragColor;

uniform float gridCount;
uniform float coarseLineWidth;
uniform float seaLevelOffset;
uniform float heightScale;
uniform float proceduralDataTexelSize;
uniform sampler2DArray proceduralHeightTexture;
uniform sampler2DArray proceduralWaterDepthTexture;

#include "planet_sampling.glsl"

float gridLineMask(float coord, float segments, float width)
{
    float scaled = coord * max(segments, 1.0);
    float distToLine = min(fract(scaled), 1.0 - fract(scaled));
    float aa = max(fwidth(scaled) * width, 1e-4);
    return 1.0 - smoothstep(0.0, aa, distToLine);
}

bool hasWaterSurface(vec3 sphereDir)
{
    float terrainHeight = sampleFloatArraySeamlessNarrow(proceduralHeightTexture, sphereDir);
    float signedWaterDepth = (seaLevelOffset - terrainHeight) * heightScale;
    float bakedWaterDepth = max(sampleFloatArraySeamless(proceduralWaterDepthTexture, sphereDir), 0.0);
    return max(signedWaterDepth, bakedWaterDepth * 0.15) > 0.001;
}

void main()
{
    if (!hasWaterSurface(normalize(teSphereDir))) {
        discard;
    }

    float coarseU = gridLineMask(teTexCoord.x, gridCount, coarseLineWidth);
    float coarseV = gridLineMask(teTexCoord.y, gridCount, coarseLineWidth);
    float coarseMask = max(coarseU, coarseV);
    float alpha = clamp(coarseMask * 0.9, 0.0, 0.9);

    if (alpha < 0.01) {
        discard;
    }

    FragColor = vec4(0.08, 0.18, 0.42, alpha);
}
