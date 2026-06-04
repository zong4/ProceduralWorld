#version 410 core

// 海洋片元阶段。
// 组合 FFT normal、细节 normal、反射/折射 FBO、深度水色、Fresnel、GGX 高光和浅水透光。

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
uniform int renderAtmosphere;
uniform float atmosphereRadius;
uniform float atmosphereDensity;
uniform float atmosphereExposure;
uniform float scatteringViewMuSize;
uniform float scatteringNuSize;
uniform float scatteringHeight;
uniform float scatteringDepth;
uniform vec3 mieColor;
uniform sampler2D reflectionTexture;
uniform sampler2D refractionTexture;
uniform sampler2D refractionDepthTexture;
uniform sampler2D oceanNormalTexture;
uniform sampler2D waterDetailNormalTextureA;
uniform sampler2D waterDetailNormalTextureB;
uniform sampler2D atmosphereIrradianceTexture;
uniform sampler3D atmosphereScatteringTexture;
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
    // OpenGL 非线性深度转线性 view-space 深度，用于计算水柱厚度。
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
    // 按球面方向在 XYZ 三个投影之间混合，避免极区 UV 拉伸。
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

bool raySphere(vec3 origin, vec3 direction, float radius, out vec2 hit)
{
    float b = dot(origin, direction);
    float c = dot(origin, origin) - radius * radius;
    float h = b * b - c;
    if (h < 0.0) {
        return false;
    }

    h = sqrt(h);
    hit = vec2(-b - h, -b + h);
    return hit.y >= 0.0;
}

float scatteringTexelU(float viewIndex, float nuIndex)
{
    return (nuIndex * scatteringViewMuSize + viewIndex + 0.5)
         / max(scatteringViewMuSize * scatteringNuSize, 1.0);
}

vec3 scatteringTexel(float viewIndex, float nuIndex, float heightIndex, float sunIndex)
{
    return texture(atmosphereScatteringTexture,
                   vec3(scatteringTexelU(viewIndex, nuIndex),
                        (heightIndex + 0.5) / max(scatteringHeight, 1.0),
                        (sunIndex + 0.5) / max(scatteringDepth, 1.0))).rgb;
}

vec3 sampleAtmosphereScattering(float viewMu, float nu, float height01, float sunMu)
{
    float viewCoord = saturate(viewMu * 0.5 + 0.5) * (scatteringViewMuSize - 1.0);
    float nuCoord = saturate(nu * 0.5 + 0.5) * (scatteringNuSize - 1.0);
    float heightCoord = saturate(height01) * (scatteringHeight - 1.0);
    float sunCoord = saturate(sunMu * 0.5 + 0.5) * (scatteringDepth - 1.0);

    float view0 = floor(viewCoord);
    float nu0 = floor(nuCoord);
    float height0 = floor(heightCoord);
    float sun0 = floor(sunCoord);
    float view1 = min(view0 + 1.0, scatteringViewMuSize - 1.0);
    float nu1 = min(nu0 + 1.0, scatteringNuSize - 1.0);
    float height1 = min(height0 + 1.0, scatteringHeight - 1.0);
    float sun1 = min(sun0 + 1.0, scatteringDepth - 1.0);

    float viewMix = fract(viewCoord);
    float nuMix = fract(nuCoord);
    float heightMix = fract(heightCoord);
    float sunMix = fract(sunCoord);

    vec3 h000 = mix(scatteringTexel(view0, nu0, height0, sun0),
                    scatteringTexel(view1, nu0, height0, sun0),
                    viewMix);
    vec3 h010 = mix(scatteringTexel(view0, nu1, height0, sun0),
                    scatteringTexel(view1, nu1, height0, sun0),
                    viewMix);
    vec3 h100 = mix(scatteringTexel(view0, nu0, height1, sun0),
                    scatteringTexel(view1, nu0, height1, sun0),
                    viewMix);
    vec3 h110 = mix(scatteringTexel(view0, nu1, height1, sun0),
                    scatteringTexel(view1, nu1, height1, sun0),
                    viewMix);
    vec3 s0 = mix(mix(h000, h010, nuMix),
                  mix(h100, h110, nuMix),
                  heightMix);

    vec3 h001 = mix(scatteringTexel(view0, nu0, height0, sun1),
                    scatteringTexel(view1, nu0, height0, sun1),
                    viewMix);
    vec3 h011 = mix(scatteringTexel(view0, nu1, height0, sun1),
                    scatteringTexel(view1, nu1, height0, sun1),
                    viewMix);
    vec3 h101 = mix(scatteringTexel(view0, nu0, height1, sun1),
                    scatteringTexel(view1, nu0, height1, sun1),
                    viewMix);
    vec3 h111 = mix(scatteringTexel(view0, nu1, height1, sun1),
                    scatteringTexel(view1, nu1, height1, sun1),
                    viewMix);
    vec3 s1 = mix(mix(h001, h011, nuMix),
                  mix(h101, h111, nuMix),
                  heightMix);

    return mix(s0, s1, sunMix);
}

vec3 applyOceanAerialPerspective(vec3 surfaceColor, vec3 worldPos)
{
    if (renderAtmosphere == 0 || atmosphereRadius <= planetRadius + 0.001 || atmosphereDensity <= 0.001) {
        return surfaceColor;
    }

    vec3 cameraToSurface = worldPos - cameraPos;
    float surfaceDistance = length(cameraToSurface);
    if (surfaceDistance <= 0.001) {
        return surfaceColor;
    }

    vec3 rayDir = cameraToSurface / surfaceDistance;
    vec2 atmosphereHit;
    if (!raySphere(cameraPos, rayDir, atmosphereRadius, atmosphereHit)) {
        return surfaceColor;
    }

    float rayStart = max(atmosphereHit.x, 0.0);
    float rayEnd = min(surfaceDistance, atmosphereHit.y);
    float rayLength = max(rayEnd - rayStart, 0.0);
    if (rayLength <= 0.001) {
        return surfaceColor;
    }

    float shellThickness = max(atmosphereRadius - planetRadius, 0.001);
    vec3 midPos = cameraPos + rayDir * (rayStart + rayLength * 0.55);
    vec3 midNormal = normalize(midPos);
    vec3 sunDir = normalize(-lightDir);
    float height01 = saturate((length(midPos) - planetRadius) / shellThickness);
    float viewMu = dot(midNormal, rayDir);
    float viewSunMu = dot(rayDir, sunDir);
    float sunMu = dot(midNormal, sunDir);

    float viewProjection = dot(cameraPos, rayDir);
    float impactRadius = sqrt(max(dot(cameraPos, cameraPos) - viewProjection * viewProjection, 0.0));
    float tangent01 = 1.0 - smoothstep(0.0, shellThickness * 2.8, abs(impactRadius - planetRadius));
    float path01 = saturate(rayLength / (shellThickness * 8.5));
    float densityHeight = exp(-height01 * 3.6);
    float airAmount = saturate((path01 * 0.44 + tangent01 * 0.32) * densityHeight * atmosphereDensity);

    vec3 scattering = sampleAtmosphereScattering(viewMu, viewSunMu, height01, sunMu);
    vec3 irradiance = texture(atmosphereIrradianceTexture, vec2(sunMu * 0.5 + 0.5, height01)).rgb;
    vec3 blueAir = vec3(0.20, 0.42, 0.88) * (0.05 + airAmount * 0.28);
    vec3 sunAir = scattering * (0.24 + saturate(viewSunMu * 0.5 + 0.5) * 0.42)
                + irradiance * 0.036
                + mieColor * pow(saturate(viewSunMu), 8.0) * 0.014;
    vec3 inScattering = (blueAir + sunAir) * airAmount * atmosphereExposure;

    vec3 extinction = vec3(0.12, 0.22, 0.48) * airAmount;
    vec3 transmittance = exp(-extinction);
    return surfaceColor * transmittance + inScattering;
}

float distributionGGX(float nDotH, float roughness)
{
    // 微表面高光项，水面 specular 使用简化 Cook-Torrance。
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
    // 从程序化地形数据判断当前海面片元是否真的覆盖水域。
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
    // 当前片元的屏幕坐标，用来采样反射/折射 render target。
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
    // FFT normal 表示大浪，detail normal 表示近景小波纹；远处逐渐淡出细节 normal。
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
    // 法线扰动屏幕 UV，形成水面折射/反射扭曲。
    vec4 reflection = texture(reflectionTexture, screenUV + distortion);
    vec4 refraction = texture(refractionTexture, screenUV - distortion * 0.5);

    float sceneDepth = linearizeDepth(texture(refractionDepthTexture, screenUV).r, cameraNearPlane, cameraFarPlane);
    float waterSurfaceDepth = linearizeDepth(gl_FragCoord.z, cameraNearPlane, cameraFarPlane);
    float waterColumnDepth = max(sceneDepth - waterSurfaceDepth, 0.0);
    // 同时使用屏幕深度和程序化水深，近景边缘稳定、远景覆盖完整。
    OceanPlanetSample planet = samplePlanet(sphereDir);
    float signedHeightWaterDepth = planet.signedWaterDepth;
    float heightWaterDepth = max(signedHeightWaterDepth, 0.0);
    if (signedHeightWaterDepth <= 0.0) {
        // 地形高于海平面处不绘制水。
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

    // Fresnel：视线越贴近水面，反射占比越高。
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

    // GGX specular 高光。
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
    // 简化次表面散射/透光：浪尖和浅水更偏青绿色。
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

    color = applyOceanAerialPerspective(color, teWorldPos);
    FragColor = vec4(toneMapAndGamma(color), waterAlpha);
}
