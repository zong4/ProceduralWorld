#version 410 core

layout(quads, fractional_even_spacing, ccw) in;

in vec2 tcTexCoord[];

out vec3 teWorldPos;
out vec3 teNormal;
out vec3 teSphereDir;
out vec2 teTexCoord;
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

vec3 triplanarWeights(vec3 sphereDir)
{
    vec3 w = pow(abs(normalize(sphereDir)), vec3(4.0));
    return w / max(w.x + w.y + w.z, 0.0001);
}

vec2 projectionUvX(vec3 sphereDir)
{
    return sphereDir.yz * 0.5 + 0.5;
}

vec2 projectionUvY(vec3 sphereDir)
{
    return sphereDir.xz * 0.5 + 0.5;
}

vec2 projectionUvZ(vec3 sphereDir)
{
    return sphereDir.xy * 0.5 + 0.5;
}

float sampleOceanFloatTriplanar(sampler2D tex, vec3 sphereDir, vec2 uvOffset)
{
    vec3 d = normalize(sphereDir);
    vec3 w = triplanarWeights(d);
    vec2 offset = uvOffset * oceanWaveTileScale;
    return texture(tex, projectionUvX(d) * oceanWaveTileScale + offset).r * w.x
         + texture(tex, projectionUvY(d) * oceanWaveTileScale + offset).r * w.y
         + texture(tex, projectionUvZ(d) * oceanWaveTileScale + offset).r * w.z;
}

vec2 sampleOceanVec2Triplanar(sampler2D tex, vec3 sphereDir)
{
    vec3 d = normalize(sphereDir);
    vec3 w = triplanarWeights(d);
    return texture(tex, projectionUvX(d) * oceanWaveTileScale).rg * w.x
         + texture(tex, projectionUvY(d) * oceanWaveTileScale).rg * w.y
         + texture(tex, projectionUvZ(d) * oceanWaveTileScale).rg * w.z;
}

vec3 longitudeTangent(vec3 sphereDir)
{
    vec3 tangent = vec3(-sphereDir.z, 0.0, sphereDir.x);
    if (dot(tangent, tangent) < 0.000001) {
        tangent = vec3(1.0, 0.0, 0.0);
    }
    return normalize(tangent);
}

void main()
{
    vec2 uv0 = mix(tcTexCoord[0], tcTexCoord[1], gl_TessCoord.x);
    vec2 uv1 = mix(tcTexCoord[3], tcTexCoord[2], gl_TessCoord.x);
    vec2 uv = mix(uv0, uv1, gl_TessCoord.y);

    teTexCoord = uv;

    vec3 sphereDir = normalize(cubeFacePoint(uv));
    float normalizedWaveHeight = sampleOceanFloatTriplanar(oceanHeightTexture, sphereDir, vec2(0.0));
    float heightRight = sampleOceanFloatTriplanar(oceanHeightTexture, sphereDir, vec2(oceanHeightTexelSize, 0.0));
    float heightUp = sampleOceanFloatTriplanar(oceanHeightTexture, sphereDir, vec2(0.0, oceanHeightTexelSize));
    float heightSlope = length(vec2(heightRight - normalizedWaveHeight, heightUp - normalizedWaveHeight));
    float waveHeight = normalizedWaveHeight * oceanWaveAmplitude;
    teWaveHeight = waveHeight;
    float crestByHeight = smoothstep(0.60, 0.90, normalizedWaveHeight * 0.5 + 0.5);
    float crestBySlope = smoothstep(0.026, 0.095, heightSlope);
    teWaveCrest = clamp(crestByHeight * (0.82 + crestBySlope * 0.36), 0.0, 1.0);
    vec3 localTangent = longitudeTangent(sphereDir);
    vec3 localBitangent = normalize(cross(sphereDir, localTangent));
    vec2 choppyDisplacement = sampleOceanVec2Triplanar(oceanDisplacementTexture, sphereDir) * oceanChoppiness;
    vec3 localPos = sphereDir * (seaLevelRadius + waveHeight)
                  + localTangent * choppyDisplacement.x
                  + localBitangent * choppyDisplacement.y;
    vec4 worldPos = model * vec4(localPos, 1.0);

    teWorldPos = worldPos.xyz;
    teNormal = normalize(mat3(transpose(inverse(model))) * sphereDir);
    teSphereDir = sphereDir;
    teTangent = normalize(mat3(model) * localTangent);
    teBitangent = normalize(mat3(model) * localBitangent);
    vec4 relativeWorldPos = vec4(worldPos.xyz - cameraPos, 1.0);
    teClipSpacePos = projection * cameraRelativeView * relativeWorldPos;
    gl_Position = teClipSpacePos;
    gl_ClipDistance[0] = dot(worldPos, clipPlane);
}
