#version 410 core

in vec3 teWorldPos;
in vec3 teNormal;
in vec3 teSphereDir;
in vec3 teTangent;
in vec3 teBitangent;
in float teWaveHeight;
in float teWaveCrest;
in vec4 teClipSpacePos;

out vec4 FragColor;

uniform mat4 view;
uniform vec3 lightDir;
uniform vec3 cameraPos;
uniform vec3 skyColor;
uniform vec3 oceanShallowColor;
uniform vec3 oceanDeepColor;
uniform vec3 oceanSSSColor;
uniform int renderMode;
uniform float planetRadius;
uniform float oceanAlpha;
uniform float oceanShallowAlpha;
uniform float oceanDeepAlpha;
uniform float oceanFresnelStrength;
uniform float oceanDistortionStrength;
uniform float oceanDepthRange;
uniform float oceanShallowDepthRange;
uniform float oceanDepthScale;
uniform float oceanTintStrength;
uniform float oceanWaveNormalStrength;
uniform float oceanWaveTileScale;
uniform float oceanDetailNormalStrength;
uniform float oceanDetailNormalScale;
uniform float oceanDetailFadeDistance;
uniform float oceanSpecularStrength;
uniform float oceanSpecularSharpness;
uniform float oceanRoughness;
uniform float oceanSSSStrength;
uniform float oceanSSSPower;
uniform float oceanShoreBlendWidth;
uniform float oceanReflectionWeight;
uniform float oceanRefractionWeight;
uniform float cameraNearPlane;
uniform float cameraFarPlane;
uniform float timeSeconds;
uniform sampler2D reflectionTexture;
uniform sampler2D refractionTexture;
uniform sampler2D refractionDepthTexture;
uniform sampler2D oceanNormalTexture;
uniform sampler2D waterDetailNormalTextureA;
uniform sampler2D waterDetailNormalTextureB;
uniform sampler2DArray proceduralWaterDepthTexture;
uniform sampler2DArray proceduralHeightTexture;
uniform float proceduralDataTexelSize;
uniform float seaLevelOffset;
uniform float heightScale;

#include "planet_sampling.glsl"

vec3 toneMapAndGamma(vec3 color)
{
    color = color / (color + vec3(1.0));
    return pow(color, vec3(1.0 / 2.2));
}

float linearizeDepth(float depthSample, float nearPlane, float farPlane)
{
    float z = depthSample * 2.0 - 1.0;
    return (2.0 * nearPlane * farPlane) / (farPlane + nearPlane - z * (farPlane - nearPlane));
}

vec3 blendTangentNormals(vec3 baseNormal, vec3 detailNormal, float detailStrength)
{
    vec2 xy = baseNormal.xz + detailNormal.xz * detailStrength;
    float y = sqrt(max(1.0 - dot(xy, xy), 0.001));
    return normalize(vec3(xy.x, y, xy.y));
}

vec3 unpackNormal(vec3 packedNormal)
{
    return normalize(packedNormal * 2.0 - 1.0);
}

vec3 unpackStandardNormalToYUp(vec3 packedNormal)
{
    vec3 normal = normalize(packedNormal * 2.0 - 1.0);
    return normalize(vec3(normal.x, normal.z, normal.y));
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

vec3 sampleRawNormalTriplanar(sampler2D tex, vec3 sphereDir, float scale, vec2 offset, float lod)
{
    vec3 d = normalize(sphereDir);
    vec3 w = triplanarWeights(d);
    vec3 normalX = normalize(textureLod(tex, projectionUvX(d) * scale + offset, lod).rgb);
    vec3 normalY = normalize(textureLod(tex, projectionUvY(d) * scale + offset, lod).rgb);
    vec3 normalZ = normalize(textureLod(tex, projectionUvZ(d) * scale + offset, lod).rgb);
    return normalize(normalX * w.x + normalY * w.y + normalZ * w.z);
}

vec3 samplePackedNormalTriplanar(sampler2D tex, vec3 sphereDir, float scale, vec2 offset, float lod)
{
    vec3 d = normalize(sphereDir);
    vec3 w = triplanarWeights(d);
    vec3 normalX = unpackStandardNormalToYUp(textureLod(tex, projectionUvX(d) * scale + offset, lod).rgb);
    vec3 normalY = unpackStandardNormalToYUp(textureLod(tex, projectionUvY(d) * scale + offset, lod).rgb);
    vec3 normalZ = unpackStandardNormalToYUp(textureLod(tex, projectionUvZ(d) * scale + offset, lod).rgb);
    return normalize(normalX * w.x + normalY * w.y + normalZ * w.z);
}

float saturate(float value)
{
    return clamp(value, 0.0, 1.0);
}

float distributionGGX(float nDotH, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float denom = nDotH * nDotH * (a2 - 1.0) + 1.0;
    return a2 / max(3.14159265 * denom * denom, 0.0001);
}

float geometrySchlickGGX(float nDotX, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return nDotX / max(nDotX * (1.0 - k) + k, 0.0001);
}

float geometrySmith(float nDotV, float nDotL, float roughness)
{
    return geometrySchlickGGX(nDotV, roughness) * geometrySchlickGGX(nDotL, roughness);
}

vec3 fresnelSchlick(float cosTheta, vec3 f0)
{
    return f0 + (1.0 - f0) * pow(1.0 - saturate(cosTheta), 5.0);
}

struct OceanPlanetSample
{
    float terrainHeight;
    float bakedWaterDepth;
    float signedWaterDepth;
    float waterDepth;
    float shoreMask;
};

OceanPlanetSample samplePlanet(vec3 sphereDir)
{
    OceanPlanetSample planetSample;
    planetSample.terrainHeight = sampleFloatArraySeamlessNarrow(proceduralHeightTexture, sphereDir);
    planetSample.bakedWaterDepth = max(sampleFloatArraySeamless(proceduralWaterDepthTexture, sphereDir), 0.0);
    planetSample.signedWaterDepth = (seaLevelOffset - planetSample.terrainHeight) * heightScale;
    planetSample.waterDepth = max(max(planetSample.signedWaterDepth, 0.0), planetSample.bakedWaterDepth * 0.15);
    planetSample.shoreMask = 1.0 - smoothstep(0.0, max(oceanShoreBlendWidth, 0.001), abs(planetSample.signedWaterDepth));
    return planetSample;
}

void main()
{
    vec2 screenUV = teClipSpacePos.xy / teClipSpacePos.w * 0.5 + 0.5;
    vec3 radialNormal = normalize(teNormal);
    vec3 tangent = normalize(teTangent);
    vec3 bitangent = normalize(teBitangent);
    vec3 sphereDir = normalize(teSphereDir);
    vec3 fftNormal = sampleRawNormalTriplanar(oceanNormalTexture, sphereDir, oceanWaveTileScale, vec2(0.0), 0.0);
    fftNormal = normalize(vec3(
        fftNormal.x * oceanWaveNormalStrength,
        max(fftNormal.y, 0.001),
        fftNormal.z * oceanWaveNormalStrength
    ));
    vec3 V = normalize(cameraPos - teWorldPos);
    vec3 L = normalize(-lightDir);

    float distanceToCamera = length(cameraPos - teWorldPos);
    float detailFade = 1.0 - smoothstep(oceanDetailFadeDistance * 0.22, oceanDetailFadeDistance * 0.72, distanceToCamera);
    detailFade *= detailFade;
    float detailLod = mix(1.0, 5.0, 1.0 - detailFade);
    mat2 detailRotation = mat2(0.8, -0.6, 0.6, 0.8);
    vec2 detailFlow = vec2(0.22, 0.11) * timeSeconds;
    vec2 counterFlow = vec2(-0.17, 0.19) * timeSeconds;
    vec3 detailNormalA = samplePackedNormalTriplanar(waterDetailNormalTextureA, sphereDir, oceanDetailNormalScale, detailFlow, detailLod);
    vec3 detailNormalB = samplePackedNormalTriplanar(waterDetailNormalTextureB, sphereDir, oceanDetailNormalScale * 1.73, detailRotation * counterFlow, detailLod + 0.5);
    vec3 detailTangentNormal = normalize(vec3(
        detailNormalA.x + detailNormalB.x * 0.45,
        detailNormalA.y * detailNormalB.y,
        detailNormalA.z + detailNormalB.z * 0.45
    ));
    vec3 finalTangentNormal = blendTangentNormals(fftNormal, detailTangentNormal, oceanDetailNormalStrength * detailFade);
    vec3 N = normalize(tangent * finalTangentNormal.x + radialNormal * finalTangentNormal.y + bitangent * finalTangentNormal.z);
    vec3 H = normalize(L + V);
    float nDotV = max(dot(N, V), 0.001);
    float nDotL = max(dot(N, L), 0.0);
    float nDotH = max(dot(N, H), 0.0);
    float vDotH = max(dot(V, H), 0.0);

    float materialDistanceFade = 1.0 - smoothstep(oceanDetailFadeDistance * 0.55, oceanDetailFadeDistance * 1.80, distanceToCamera);
    vec3 viewNormal = normalize(mat3(view) * N);
    vec2 distortion = viewNormal.xy * oceanDistortionStrength * mix(0.18, 1.0, materialDistanceFade);
    vec4 reflection = texture(reflectionTexture, screenUV + distortion);
    vec4 refraction = texture(refractionTexture, screenUV - distortion * 0.5);

    float sceneDepth = linearizeDepth(texture(refractionDepthTexture, screenUV).r, cameraNearPlane, cameraFarPlane);
    float waterSurfaceDepth = linearizeDepth(gl_FragCoord.z, cameraNearPlane, cameraFarPlane);
    float waterColumnDepth = max(sceneDepth - waterSurfaceDepth, 0.0);
    OceanPlanetSample planet = samplePlanet(sphereDir);
    float signedHeightWaterDepth = planet.signedWaterDepth;
    float heightWaterDepth = max(signedHeightWaterDepth, 0.0);
    if (signedHeightWaterDepth <= 0.0) {
        discard;
    }
    float proceduralWaterDepth = planet.waterDepth;
    float runtimeShore = planet.shoreMask;
    float depthPixelWidth = max(fwidth(heightWaterDepth) * 2.0, max(oceanShoreBlendWidth, heightScale * proceduralDataTexelSize * 0.75));
    float nearWaterCoverage = smoothstep(0.0, depthPixelWidth, heightWaterDepth);
    nearWaterCoverage = max(nearWaterCoverage, runtimeShore * smoothstep(0.0, depthPixelWidth, proceduralWaterDepth) * 0.22);
    float farOceanCoverage = smoothstep(planetRadius * 3.5, planetRadius * 8.0, distanceToCamera);
    float farWaterCoverage = smoothstep(0.0, max(depthPixelWidth, oceanShoreBlendWidth * 1.5), proceduralWaterDepth);
    float waterCoverage = mix(nearWaterCoverage, farWaterCoverage, farOceanCoverage);

    if (waterCoverage <= 0.01) {
        discard;
    }

    float visualWaterDepth = proceduralWaterDepth * oceanDepthScale;
    float depthBlend = clamp(min(waterColumnDepth * oceanDepthScale, visualWaterDepth) / max(oceanDepthRange, 0.001), 0.0, 1.0);
    float shallowDepth = clamp(visualWaterDepth / max(oceanShallowDepthRange, 0.001), 0.0, 1.0);

    vec3 waterF0 = vec3(0.0204);
    vec3 fresnelColor = fresnelSchlick(nDotV, waterF0);
    float fresnel = clamp(fresnelColor.r * oceanFresnelStrength, 0.02, 1.0);

    float colorDepth = smoothstep(0.02, 0.92, min(depthBlend, shallowDepth));
    vec3 depthTint = mix(oceanShallowColor, oceanDeepColor, colorDepth);
    vec3 baseTint = mix(oceanShallowColor, depthTint, 0.82);
    vec3 refractionSource = mix(baseTint, refraction.rgb, clamp(oceanRefractionWeight, 0.0, 1.0));
    vec3 reflectionSource = mix(skyColor, reflection.rgb, clamp(oceanReflectionWeight, 0.0, 1.0));
    vec3 refractedColor = mix(refractionSource, baseTint, oceanTintStrength * (0.48 + colorDepth * 0.18));
    vec3 reflectedColor = mix(reflectionSource, skyColor, 0.05);

    float diffuse = nDotL;
    float alphaDepth = smoothstep(0.0, 1.0, shallowDepth);
    float waterAlpha = mix(oceanShallowAlpha, oceanDeepAlpha, alphaDepth);
    waterAlpha = clamp(waterAlpha * oceanAlpha * waterCoverage, 0.0, 1.0);

    vec3 color = mix(refractedColor, reflectedColor, fresnel);

    float roughness = oceanRoughness;
    roughness = clamp(roughness / max(oceanSpecularSharpness, 0.001), 0.025, 1.0);
    float D = distributionGGX(nDotH, roughness);
    float G = geometrySmith(nDotV, nDotL, roughness);
    vec3 F = fresnelSchlick(vDotH, waterF0);
    vec3 specular = (D * G * F) / max(4.0 * nDotV * max(nDotL, 0.001), 0.0001);
    specular *= oceanSpecularStrength * mix(0.18, 1.0, detailFade);

    float viewBackLight = pow(saturate(dot(V, -L) * 0.5 + 0.5), oceanSSSPower);
    float lightWrap = pow(saturate(dot(N, -L) * 0.5 + 0.5), 1.4);
    float crestTranslucency = pow(saturate(teWaveCrest), 1.15);
    float shallowTranslucency = pow(saturate(1.0 - shallowDepth), 2.2) * 0.35;
    float sssMask = saturate((crestTranslucency + shallowTranslucency) * mix(lightWrap, viewBackLight, 0.45));
    vec3 sss = oceanSSSColor * sssMask * oceanSSSStrength;
    color = mix(color, oceanSSSColor, sssMask * oceanSSSStrength * 0.22);

    if (renderMode == 1) {
        vec3 unshadedWater = mix(oceanShallowColor, oceanDeepColor, clamp(teWaveCrest * 0.65 + 0.18, 0.0, 1.0));
        unshadedWater = mix(unshadedWater, oceanSSSColor, sssMask * oceanSSSStrength * 0.35);
        unshadedWater += oceanSSSColor * sssMask * oceanSSSStrength * 0.35;
        FragColor = vec4(unshadedWater, waterAlpha);
        return;
    }

    if (renderMode == 3) {
        FragColor = vec4(N * 0.5 + 0.5, waterAlpha);
        return;
    }

    color += depthTint * (0.08 + 0.18 * diffuse);
    color += sss;
    color += vec3(1.0, 0.98, 0.94) * specular;

    FragColor = vec4(toneMapAndGamma(color), waterAlpha);
}
