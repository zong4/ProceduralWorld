#version 410 core

in vec3 teSphereDir;

out vec4 FragColor;

uniform float seaLevelOffset;
uniform float heightScale;
uniform float proceduralDataTexelSize;
uniform sampler2DArray proceduralHeightTexture;
uniform sampler2DArray proceduralWaterDepthTexture;

#include "planet_sampling.glsl"

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

    FragColor = vec4(1.0, 0.55, 0.10, 0.45);
}
