#version 410 core

layout(quads, fractional_even_spacing, ccw) in;

in vec2 tcTexCoord[];

out vec3 teWorldPos;
out vec3 teNormal;
out vec2 teTexCoord;
out vec2 teWaveUv;
out vec2 teFoamCoord;
out vec3 teTangent;
out vec3 teBitangent;
out float teWaveHeight;
out float teWaveCrest;
out vec4 teClipSpacePos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 cameraRelativeView;
uniform mat4 projection;
uniform vec3 cameraPos;
uniform vec3 faceNormal;
uniform vec3 faceAxisU;
uniform vec3 faceAxisV;
uniform float seaLevelRadius;
uniform float oceanWaveAmplitude;
uniform float oceanChoppiness;
uniform float oceanWaveTileScale;
uniform float oceanHeightTexelSize;
uniform sampler2D oceanHeightTexture;
uniform sampler2D oceanDisplacementTexture;
uniform vec4 clipPlane;

vec3 cubeFacePoint(vec2 uv)
{
    vec2 faceUV = uv * 2.0 - 1.0;
    return faceNormal + faceUV.x * faceAxisU + faceUV.y * faceAxisV;
}

void main()
{
    vec2 uv0 = mix(tcTexCoord[0], tcTexCoord[1], gl_TessCoord.x);
    vec2 uv1 = mix(tcTexCoord[3], tcTexCoord[2], gl_TessCoord.x);
    vec2 uv = mix(uv0, uv1, gl_TessCoord.y);

    teTexCoord = uv;

    vec3 sphereDir = normalize(cubeFacePoint(uv));
    teWaveUv = uv * oceanWaveTileScale;
    teFoamCoord = (uv * 2.0 - 1.0) * seaLevelRadius;
    float normalizedWaveHeight = texture(oceanHeightTexture, teWaveUv).r;
    float heightRight = texture(oceanHeightTexture, teWaveUv + vec2(oceanHeightTexelSize, 0.0)).r;
    float heightUp = texture(oceanHeightTexture, teWaveUv + vec2(0.0, oceanHeightTexelSize)).r;
    float heightSlope = length(vec2(heightRight - normalizedWaveHeight, heightUp - normalizedWaveHeight));
    float waveHeight = normalizedWaveHeight * oceanWaveAmplitude;
    teWaveHeight = waveHeight;
    float crestByHeight = smoothstep(0.58, 0.92, normalizedWaveHeight * 0.5 + 0.5);
    float crestBySlope = smoothstep(0.015, 0.085, heightSlope);
    teWaveCrest = clamp(crestByHeight * 0.72 + crestBySlope * 0.36, 0.0, 1.0);
    vec3 localTangent = normalize(faceAxisU - sphereDir * dot(faceAxisU, sphereDir));
    vec3 localBitangent = normalize(cross(sphereDir, localTangent));
    if (dot(localBitangent, faceAxisV) < 0.0) {
        localBitangent = -localBitangent;
    }
    vec2 choppyDisplacement = texture(oceanDisplacementTexture, teWaveUv).rg * oceanChoppiness;
    vec3 localPos = sphereDir * (seaLevelRadius + waveHeight)
                  + localTangent * choppyDisplacement.x
                  + localBitangent * choppyDisplacement.y;
    vec4 worldPos = model * vec4(localPos, 1.0);

    teWorldPos = worldPos.xyz;
    teNormal = normalize(mat3(transpose(inverse(model))) * sphereDir);
    teTangent = normalize(mat3(model) * localTangent);
    teBitangent = normalize(mat3(model) * localBitangent);
    vec4 relativeWorldPos = vec4(worldPos.xyz - cameraPos, 1.0);
    teClipSpacePos = projection * cameraRelativeView * relativeWorldPos;
    gl_Position = teClipSpacePos;
    gl_ClipDistance[0] = dot(worldPos, clipPlane);
}
