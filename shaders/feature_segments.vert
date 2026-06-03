#version 410 core

layout (location = 0) in vec3 aSphereDir;
layout (location = 1) in float aStrength;

out float vStrength;

uniform mat4 model;
uniform mat4 cameraRelativeView;
uniform mat4 projection;
uniform vec3 cameraPos;
uniform float planetRadius;
uniform float heightScale;
uniform float seaLevelOffset;
uniform float runtimeMountainScale;
uniform float lineLift;
uniform float proceduralDataTexelSize;
uniform sampler2DArray proceduralHeightTexture;

#include "planet_sampling.glsl"

void main()
{
    vec3 sphereDir = normalize(aSphereDir);
    float baseHeight = sampleFloatArraySeamlessNarrow(proceduralHeightTexture, sphereDir);
    
    float baseHeightOffset = baseHeight - seaLevelOffset;
    float mountainOffset = max(baseHeightOffset, 0.0) * (runtimeMountainScale - 1.0);
    float displacedHeight = baseHeight + mountainOffset;

    vec3 localPos = sphereDir * (planetRadius + displacedHeight * heightScale + lineLift);
    vec4 worldPos = model * vec4(localPos, 1.0);
    vec4 relativeWorldPos = vec4(worldPos.xyz - cameraPos, 1.0);

    vStrength = clamp(aStrength, 0.0, 1.0);
    gl_Position = projection * cameraRelativeView * relativeWorldPos;
}
