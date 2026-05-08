#version 410 core

in vec3 teWorldPos;
in vec3 teNormal;
in vec3 teSphereDir;
in vec2 teTexCoord;
in vec2 teWaveUv;
in vec2 teWaterCoord;
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
uniform int faceIndex;
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
    vec3 V = normalize(cameraPos - teWorldPos);
    vec3 L = normalize(-lightDir);

    float distanceToCamera = length(cameraPos - teWorldPos);
    float detailFade = 1.0 - smoothstep(oceanDetailFadeDistance * 0.22, oceanDetailFadeDistance * 0.72, distanceToCamera);
    detailFade *= detailFade;
    float detailLod = mix(1.0, 5.0, 1.0 - detailFade);
    mat2 detailRotation = mat2(0.8, -0.6, 0.6, 0.8);
    vec2 detailCoord = teWaterCoord * (oceanDetailNormalScale * 0.015);
    vec2 detailFlow = vec2(0.22, 0.11) * timeSeconds;
    vec2 counterFlow = vec2(-0.17, 0.19) * timeSeconds;
    vec2 detailUvA = detailCoord + detailFlow;
    vec2 detailUvB = detailRotation * (detailCoord * 1.73) + counterFlow;
    vec3 detailNormalA = unpackStandardNormalToYUp(textureLod(waterDetailNormalTextureA, detailUvA, detailLod).rgb);
    vec3 detailNormalB = unpackStandardNormalToYUp(textureLod(waterDetailNormalTextureB, detailUvB, detailLod + 0.5).rgb);
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
    vec3 proceduralUv = vec3(teTexCoord, float(faceIndex));
    float proceduralWaterDepth = max(texture(proceduralWaterDepthTexture, proceduralUv).r, 0.0);
    float depthPixelWidth = max(fwidth(proceduralWaterDepth) * 2.0, oceanShoreBlendWidth);
    depthPixelWidth *= mix(0.75, 1.85, smoothstep(280.0, 1600.0, distanceToCamera));
    float waterCoverage = smoothstep(depthPixelWidth * 0.25, depthPixelWidth, proceduralWaterDepth);

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
