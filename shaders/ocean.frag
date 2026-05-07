#version 410 core

in vec3 teWorldPos;
in vec3 teNormal;
in vec2 teTexCoord;
in vec2 teWaveUv;
in vec2 teFoamCoord;
in vec3 teTangent;
in vec3 teBitangent;
in float teWaveHeight;
in float teWaveCrest;
in vec4 teClipSpacePos;

out vec4 FragColor;

uniform mat4 view;
uniform vec3 lightDir;
uniform vec3 cameraPos;
uniform vec3 oceanShallowColor;
uniform vec3 oceanDeepColor;
uniform vec3 oceanFoamColor;
uniform int renderMode;
uniform float oceanAlpha;
uniform float oceanFresnelStrength;
uniform float oceanDistortionStrength;
uniform float oceanDepthRange;
uniform float oceanWaveNormalStrength;
uniform float oceanDetailNormalStrength;
uniform float oceanDetailNormalScale;
uniform float oceanDetailFadeDistance;
uniform float oceanSpecularStrength;
uniform float oceanSpecularSharpness;
uniform float oceanFoamAmount;
uniform float oceanFoamThreshold;
uniform float oceanFoamSoftness;
uniform float oceanFoamScale;
uniform float oceanFoamNoiseStrength;
uniform float oceanFoamCrestPower;
uniform float oceanFoamSlopeWeight;
uniform float oceanFoamFoldWeight;
uniform float oceanFoamFadeDistance;
uniform vec2 oceanWindDirection;
uniform float timeSeconds;
uniform sampler2D reflectionTexture;
uniform sampler2D refractionTexture;
uniform sampler2D refractionDepthTexture;
uniform sampler2D oceanNormalTexture;
uniform sampler2D oceanFoldingTexture;
uniform sampler2D waterDetailNormalTextureA;
uniform sampler2D waterDetailNormalTextureB;
uniform sampler2D foamNoiseTexture;

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

void main()
{
    vec2 screenUV = teClipSpacePos.xy / teClipSpacePos.w * 0.5 + 0.5;
    vec3 radialNormal = normalize(teNormal);
    vec3 tangent = normalize(teTangent);
    vec3 bitangent = normalize(teBitangent);
    vec3 fftNormal = normalize(texture(oceanNormalTexture, teWaveUv).rgb);
    fftNormal = normalize(vec3(
        fftNormal.x * oceanWaveNormalStrength,
        max(fftNormal.y, 0.001),
        fftNormal.z * oceanWaveNormalStrength
    ));
    vec3 fftWorldNormal = normalize(tangent * fftNormal.x + radialNormal * fftNormal.y + bitangent * fftNormal.z);
    vec3 V = normalize(cameraPos - teWorldPos);
    vec3 L = normalize(-lightDir);

    vec2 detailFlow = vec2(0.22, 0.11) * timeSeconds;
    vec2 counterFlow = vec2(-0.17, 0.19) * timeSeconds;
    vec3 detailNormalA = unpackStandardNormalToYUp(texture(waterDetailNormalTextureA, teWaveUv * oceanDetailNormalScale + detailFlow).rgb);
    vec3 detailNormalB = unpackStandardNormalToYUp(texture(waterDetailNormalTextureB, teWaveUv * oceanDetailNormalScale * 1.83 + counterFlow).rgb);
    vec3 detailTangentNormal = normalize(vec3(
        detailNormalA.x + detailNormalB.x * 0.55,
        detailNormalA.y * detailNormalB.y,
        detailNormalA.z + detailNormalB.z * 0.55
    ));
    float detailFade = clamp(1.0 - length(cameraPos - teWorldPos) / max(oceanDetailFadeDistance, 0.001), 0.0, 1.0);
    vec3 finalTangentNormal = blendTangentNormals(fftNormal, detailTangentNormal, oceanDetailNormalStrength * detailFade);
    vec3 N = normalize(tangent * finalTangentNormal.x + radialNormal * finalTangentNormal.y + bitangent * finalTangentNormal.z);
    vec3 H = normalize(L + V);

    vec3 viewNormal = normalize(mat3(view) * N);
    vec2 distortion = viewNormal.xy * oceanDistortionStrength;
    vec4 reflection = texture(reflectionTexture, screenUV + distortion);
    vec4 refraction = texture(refractionTexture, screenUV - distortion * 0.5);

    float sceneDepth = linearizeDepth(texture(refractionDepthTexture, screenUV).r, 0.05, 500.0);
    float waterSurfaceDepth = linearizeDepth(gl_FragCoord.z, 0.05, 500.0);
    float waterColumnDepth = max(sceneDepth - waterSurfaceDepth, 0.0);
    float depthBlend = clamp(waterColumnDepth / max(oceanDepthRange, 0.001), 0.0, 1.0);

    float fresnel = pow(1.0 - max(dot(N, V), 0.0), 4.0);
    fresnel = clamp(0.08 + fresnel * (0.92 * oceanFresnelStrength), 0.08, 1.0);

    vec3 depthTint = mix(oceanShallowColor, oceanDeepColor, depthBlend);
    vec3 refractedColor = mix(refraction.rgb, depthTint, 0.55 + depthBlend * 0.25);
    vec3 skyReflection = vec3(0.53, 0.73, 0.94);
    vec3 reflectedColor = mix(reflection.rgb, skyReflection, 0.12);

    float diffuse = max(dot(N, L), 0.0);

    float slope = 1.0 - clamp(dot(fftWorldNormal, radialNormal), 0.0, 1.0);
    float crestFoam = pow(clamp(teWaveCrest, 0.0, 1.0), oceanFoamCrestPower);
    float slopeFoam = smoothstep(0.16, 0.40, slope);
    float folding = texture(oceanFoldingTexture, teWaveUv).r;
    float foldFoam = smoothstep(0.30, 0.72, folding);
    float foamBase = crestFoam * (1.0 + slopeFoam * oceanFoamSlopeWeight);
    foamBase *= mix(0.90, 1.15, foldFoam * oceanFoamFoldWeight);
    foamBase = clamp(foamBase, 0.0, 1.0);

    vec2 fftWind = normalize(oceanWindDirection);
    vec3 globalWind = normalize(vec3(fftWind.x, 0.0, fftWind.y));
    vec3 windTangent3D = globalWind - radialNormal * dot(globalWind, radialNormal);
    windTangent3D = normalize(length(windTangent3D) > 0.001 ? windTangent3D : tangent);
    vec2 windDir = normalize(vec2(dot(windTangent3D, tangent), dot(windTangent3D, bitangent)));
    vec2 sideDir = vec2(-windDir.y, windDir.x);
    float alongWind = dot(teFoamCoord, windDir);
    float sideWind = dot(teFoamCoord, sideDir);
    vec2 stretchedFoamUv = vec2(
        alongWind * oceanFoamScale * 0.65,
        sideWind * oceanFoamScale * 1.80
    );
    stretchedFoamUv += vec2(timeSeconds * 0.035, timeSeconds * 0.010);

    float distanceToCamera = length(cameraPos - teWorldPos);
    float nearFoamFade = clamp(1.0 - distanceToCamera / max(oceanFoamFadeDistance, 0.001), 0.0, 1.0);
    float farFoamFade = clamp(1.0 - distanceToCamera / max(oceanFoamFadeDistance * 3.0, 0.001), 0.0, 1.0);
    float lowNoise = textureLod(foamNoiseTexture, stretchedFoamUv * 0.60, 1.0).r;
    float highNoise = textureLod(
        foamNoiseTexture,
        stretchedFoamUv * 1.80 + vec2(timeSeconds * 0.030, timeSeconds * 0.012),
        0.0
    ).r;
    float foamField = foamBase * mix(0.92, 1.08, lowNoise);
    float brokenThreshold = oceanFoamThreshold + (highNoise - 0.5) * oceanFoamNoiseStrength;
    float foamMask = smoothstep(
        brokenThreshold,
        brokenThreshold + oceanFoamSoftness,
        foamField
    );
    foamMask = clamp(foamMask * oceanFoamAmount * mix(farFoamFade, nearFoamFade, 0.35), 0.0, 1.0);
    float foamVisual = pow(foamMask, 1.15);
    float foamCore = smoothstep(0.65, 1.0, foamMask);

    fresnel *= 1.0 - foamMask * 0.50;
    vec3 color = mix(refractedColor, reflectedColor, fresnel);

    float specularPower = mix(140.0, 420.0, detailFade) * oceanSpecularSharpness;
    float specular = pow(max(dot(N, H), 0.0), specularPower) * mix(0.08, 0.55, detailFade) * oceanSpecularStrength;
    specular *= 1.0 - foamMask * 0.72;

    if (renderMode == 1) {
        vec3 unshadedWater = mix(oceanShallowColor, oceanDeepColor, clamp(teWaveCrest * 0.65 + 0.18, 0.0, 1.0));
        unshadedWater = mix(unshadedWater, oceanFoamColor, foamVisual * 0.75);
        FragColor = vec4(unshadedWater, oceanAlpha);
        return;
    }

    if (renderMode == 3) {
        FragColor = vec4(N * 0.5 + 0.5, oceanAlpha);
        return;
    }

    color += depthTint * (0.22 + 0.30 * diffuse);
    color += vec3(1.0, 0.98, 0.94) * specular;
    color = mix(color, oceanFoamColor, foamVisual * 0.65);
    color += oceanFoamColor * foamCore * 0.25;

    FragColor = vec4(toneMapAndGamma(color), oceanAlpha);
}
