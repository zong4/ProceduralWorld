#version 410 core

layout(quads, fractional_even_spacing, ccw) in;

in vec2 tcTexCoord[];
in float tcSkirt[];

out vec3 teWorldPos;
out vec3 teNormal;
out vec2 teTexCoord;
out vec3 teSphereDir;
out float teHeight;
out float teSurfaceHeight;
out float teSkirt;
out vec4 teClipSpacePos;

uniform mat4 model;
uniform mat4 cameraRelativeView;
uniform mat4 projection;
uniform vec3 cameraPos;
uniform vec3 faceNormal;
uniform vec3 faceAxisU;
uniform vec3 faceAxisV;
uniform float planetRadius;
uniform float heightScale;
uniform float terrainSkirtDepth;
uniform float noiseScale;
uniform float mountainMaskStrength;
uniform float mountainMaskScale;
uniform float mountainRidgeSharpness;
uniform float seaLevelOffset;
uniform float terrainShoreLift;
uniform float oceanShoreBlendWidth;
uniform float regionalDetailStrength;
uniform float microDetailStrength;
uniform float regionalDetailStartAltitude;
uniform float regionalDetailEndAltitude;
uniform float microDetailStartAltitude;
uniform float microDetailEndAltitude;
uniform float cameraAltitude;
uniform vec4 clipPlane;
uniform float proceduralDataTexelSize;
uniform sampler2DArray proceduralHeightTexture;
uniform sampler2DArray proceduralBiomeWeightATexture;
uniform sampler2DArray proceduralBiomeWeightBTexture;

vec3 cubeFacePoint(vec2 uv)
{
    vec2 faceUV = uv * 2.0 - 1.0;
    return faceNormal + faceUV.x * faceAxisU + faceUV.y * faceAxisV;
}

#include "planet_sampling.glsl"

float hash31(vec3 p)
{
    return fract(sin(dot(p, vec3(127.1, 311.7, 74.7))) * 43758.5453123);
}

float valueNoise(vec3 p)
{
    vec3 i = floor(p);
    vec3 f = fract(p);
    vec3 u = f * f * (3.0 - 2.0 * f);

    float n000 = hash31(i + vec3(0.0, 0.0, 0.0));
    float n100 = hash31(i + vec3(1.0, 0.0, 0.0));
    float n010 = hash31(i + vec3(0.0, 1.0, 0.0));
    float n110 = hash31(i + vec3(1.0, 1.0, 0.0));
    float n001 = hash31(i + vec3(0.0, 0.0, 1.0));
    float n101 = hash31(i + vec3(1.0, 0.0, 1.0));
    float n011 = hash31(i + vec3(0.0, 1.0, 1.0));
    float n111 = hash31(i + vec3(1.0, 1.0, 1.0));

    float nx00 = mix(n000, n100, u.x);
    float nx10 = mix(n010, n110, u.x);
    float nx01 = mix(n001, n101, u.x);
    float nx11 = mix(n011, n111, u.x);
    float nxy0 = mix(nx00, nx10, u.y);
    float nxy1 = mix(nx01, nx11, u.y);
    return mix(nxy0, nxy1, u.z);
}

float fbm3(vec3 p)
{
    float value = 0.0;
    float amplitude = 0.5;
    float total = 0.0;

    for (int i = 0; i < 4; ++i) {
        value += valueNoise(p) * amplitude;
        total += amplitude;
        p *= 2.03;
        amplitude *= 0.5;
    }

    return value / max(total, 0.0001);
}

float gradientLikeNoise(vec3 p)
{
    return valueNoise(p) * 2.0 - 1.0;
}

float samplePlanetHeight(vec3 sphereDir)
{
    return sampleFloatArraySeamlessNarrow(proceduralHeightTexture, sphereDir);
}

float terrainHeightAtDir(vec3 sphereDir)
{
    return samplePlanetHeight(sphereDir);
}

vec4 samplePlanetBiomeA(vec3 sphereDir)
{
    return clamp(sampleVec4ArraySeamless(proceduralBiomeWeightATexture, sphereDir), vec4(0.0), vec4(1.0));
}

vec4 samplePlanetBiomeB(vec3 sphereDir)
{
    return clamp(sampleVec4ArraySeamless(proceduralBiomeWeightBTexture, sphereDir), vec4(0.0), vec4(1.0));
}

float runtimeShoreMask(float h)
{
    float signedWaterDepth = (seaLevelOffset - h) * heightScale;
    return 1.0 - smoothstep(0.0, max(oceanShoreBlendWidth, 0.001), abs(signedWaterDepth));
}

float runtimeTerrainDetail(vec3 sphereDir, float baseHeight)
{
    float regionalWeight = regionalDetailStrength * (1.0 - smoothstep(
        regionalDetailStartAltitude,
        regionalDetailEndAltitude,
        cameraAltitude
    ));
    float ridgeWeight = regionalDetailStrength * (1.0 - smoothstep(
        regionalDetailStartAltitude * 0.45,
        regionalDetailEndAltitude * 0.75,
        cameraAltitude
    ));
    float microWeight = microDetailStrength * (1.0 - smoothstep(
        microDetailStartAltitude,
        microDetailEndAltitude,
        cameraAltitude
    ));

    vec3 p = sphereDir * noiseScale;
    vec3 warp = vec3(
        fbm3(p + vec3(3.1, 0.0, 0.0)),
        fbm3(p + vec3(0.0, 4.7, 0.0)),
        fbm3(p + vec3(0.0, 0.0, 5.3))
    );

    float landMask = smoothstep(seaLevelOffset - 0.015, seaLevelOffset + 0.075, baseHeight);
    float shoreDetailFade = smoothstep(0.018, 0.115, abs(baseHeight - seaLevelOffset));
    float landDetailMask = landMask * shoreDetailFade;
    float highlandMask = smoothstep(seaLevelOffset + 0.10, seaLevelOffset + 0.55, baseHeight);
    float continentalShelf = smoothstep(seaLevelOffset - 0.22, seaLevelOffset + 0.36, baseHeight);
    vec3 mountainP = p * mountainMaskScale + warp * 1.35;
    float mountainBands = 1.0 - abs(gradientLikeNoise(mountainP));
    mountainBands = pow(clamp(mountainBands, 0.0, 1.0), 1.85);
    float mountainField = fbm3(mountainP * 0.72 + vec3(11.7, 2.3, 6.1));
    float mountainMask = smoothstep(0.40, 0.74, mountainBands * 0.68 + mountainField * 0.32);
    mountainMask = max(mountainMask, highlandMask * 0.42);
    mountainMask = pow(clamp(mountainMask * continentalShelf, 0.0, 1.0), 1.15);

    float massifNoise = fbm3(p * 1.35 + warp * 0.9 + vec3(14.3, 5.2, 8.7)) * 2.0 - 1.0;
    float massifMask = smoothstep(0.04, 0.54, massifNoise) * mountainMask;
    massifMask = pow(clamp(massifMask, 0.0, 1.0), 1.08);
    float massifSecondary = fbm3(p * 2.6 + warp * 1.3 + vec3(6.4, 17.8, 3.1)) * 2.0 - 1.0;
    float massifHeight = massifMask * (0.22 + massifSecondary * 0.045);

    float ridges = 1.0 - abs(gradientLikeNoise(p * 7.0 + warp * 3.4));
    ridges = pow(clamp(ridges, 0.0, 1.0), mountainRidgeSharpness);
    float fineRidges = 1.0 - abs(gradientLikeNoise(p * 22.0 + warp * 7.5));
    fineRidges = pow(clamp(fineRidges, 0.0, 1.0), mountainRidgeSharpness + 1.65);
    float fineRidgeBreakup = fbm3(p * 34.0 + warp * 9.0 + vec3(17.3, 4.8, 9.6));
    float detailPeaks = fineRidges * smoothstep(0.36, 0.82, fineRidgeBreakup);
    float summitMask = smoothstep(seaLevelOffset + 0.42, seaLevelOffset + 0.88, baseHeight);
    summitMask = max(summitMask, mountainMask * highlandMask);
    float alpineFold = 1.0 - abs(gradientLikeNoise(p * 12.5 + warp * 4.8 + vec3(5.1, 13.7, 2.4)));
    alpineFold = pow(clamp(alpineFold, 0.0, 1.0), max(mountainRidgeSharpness - 0.35, 1.0));
    float alpineBreakup = fbm3(p * 18.0 + warp * 6.5 + vec3(23.4, 7.2, 14.9));
    float summitPeaks = alpineFold * smoothstep(0.48, 0.86, alpineBreakup);
    float regional = fbm3(p * 4.2 + warp * 2.5) * 2.0 - 1.0;
    float rollingDetail = fbm3(p * 2.2 + warp * 1.6) * 2.0 - 1.0;
    float micro = fbm3(p * 15.0 + warp * 5.0) * 2.0 - 1.0;
    vec4 biomeA = samplePlanetBiomeA(sphereDir);
    vec4 biomeB = samplePlanetBiomeB(sphereDir);
    float beachBiome = biomeA.r;
    float grassBiome = biomeA.g;
    float forestBiome = biomeA.b;
    float desertBiome = biomeA.a;
    float rockBiome = biomeB.r;
    float snowBiome = biomeB.g;
    float wetlandBiome = biomeB.b;
    float shallowWaterBiome = biomeB.a;

    float detail = 0.0;
    detail += ridgeWeight * rollingDetail * 0.08 * (1.0 - mountainMask * 0.50);
    detail += regionalWeight * mountainMaskStrength * massifHeight;
    detail += ridgeWeight * mountainMaskStrength * mountainMask * (ridges * 0.26 + regional * 0.10);
    detail += ridgeWeight * mountainMaskStrength * summitMask * (summitPeaks * 0.055 + alpineFold * 0.030);
    detail += microWeight * (micro * 0.050 + ridges * mountainMask * 0.045);
    detail += microWeight * mountainMaskStrength * mountainMask * (detailPeaks * 0.060 + fineRidges * highlandMask * 0.020);
    detail += microWeight * mountainMaskStrength * summitMask * (detailPeaks * 0.050 + summitPeaks * 0.055 + fineRidges * 0.025);
    float softBiomeDamp = beachBiome * 0.34
                         + grassBiome * 0.10
                         + forestBiome * 0.20
                         + wetlandBiome * 0.62
                         + shallowWaterBiome * 0.72;
    detail *= 1.0 - clamp(softBiomeDamp, 0.0, 0.82);

    float exposedRock = clamp(rockBiome + snowBiome * 0.45, 0.0, 1.0);
    detail += ridgeWeight
            * mountainMaskStrength
            * exposedRock
            * landDetailMask
            * (ridges * 0.045 + fineRidges * highlandMask * 0.040 + detailPeaks * 0.035);

    vec3 duneAxis = normalize(vec3(0.78, 0.18, 0.60));
    float duneBands = sin(dot(sphereDir, duneAxis) * 95.0 + fbm3(p * 5.8 + warp * 1.2) * 6.0);
    float duneBreakup = fbm3(p * 10.0 + vec3(19.4, 3.1, 11.8)) * 2.0 - 1.0;
    float duneDetail = duneBands * 0.018 + duneBreakup * 0.010;
    detail += desertBiome * landDetailMask * (1.0 - mountainMask * 0.70) * microWeight * duneDetail;

    float wetlandFlatten = wetlandBiome * landDetailMask * smoothstep(seaLevelOffset - 0.025, seaLevelOffset + 0.075, baseHeight);
    detail -= wetlandFlatten * 0.018;
    detail *= landDetailMask;
    float waterDepth = seaLevelOffset - baseHeight;
    float oceanMask = smoothstep(0.0, 0.075, waterDepth);
    float deepOceanMask = smoothstep(0.18, 0.62, waterDepth);
    float seabedUndulation = fbm3(p * 3.1 + warp * 1.4 + vec3(41.2, 7.6, 15.9)) * 2.0 - 1.0;
    float seabedRidges = 1.0 - abs(gradientLikeNoise(p * 8.4 + warp * 3.0 + vec3(12.3, 27.4, 5.8)));
    seabedRidges = pow(clamp(seabedRidges, 0.0, 1.0), 3.4);
    detail += oceanMask * (seabedUndulation * 0.035 + seabedRidges * deepOceanMask * 0.045);
    return detail;
}

float finalTerrainHeightAtDir(vec3 sphereDir)
{
    sphereDir = normalize(sphereDir);
    float baseHeight = terrainHeightAtDir(sphereDir);
    float h = baseHeight + runtimeTerrainDetail(sphereDir, baseHeight);
    if (baseHeight < seaLevelOffset) {
        float waterDepth = seaLevelOffset - baseHeight;
        float submergeMargin = mix(0.001, 0.012, smoothstep(0.0, 0.10, waterDepth));
        h = min(h, seaLevelOffset - submergeMargin);
    }
    float shoreWidth = max(oceanShoreBlendWidth / max(heightScale, 0.0001), 0.001);
    float landMask = smoothstep(0.0, shoreWidth * 0.40, h - seaLevelOffset);
    h += runtimeShoreMask(h) * landMask * terrainShoreLift;
    return h;
}

float finalTerrainHeightAtUv(vec2 uv)
{
    vec3 sphereDir = normalize(cubeFacePoint(uv));
    return finalTerrainHeightAtDir(sphereDir);
}

vec3 displacedPositionFromDir(vec3 sphereDir)
{
    sphereDir = normalize(sphereDir);
    float h = finalTerrainHeightAtDir(sphereDir);
    return sphereDir * (planetRadius + h * heightScale);
}

vec3 computeSphericalNormal(vec3 sphereDir)
{
    vec3 n = normalize(sphereDir);
    vec3 up = abs(n.y) < 0.9 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent = normalize(cross(up, n));
    vec3 bitangent = normalize(cross(n, tangent));
    float eps = max(proceduralDataTexelSize * 1.5, 0.0035);

    vec3 dL = normalize(n - tangent * eps);
    vec3 dR = normalize(n + tangent * eps);
    vec3 dD = normalize(n - bitangent * eps);
    vec3 dU = normalize(n + bitangent * eps);

    vec3 pL = displacedPositionFromDir(dL);
    vec3 pR = displacedPositionFromDir(dR);
    vec3 pD = displacedPositionFromDir(dD);
    vec3 pU = displacedPositionFromDir(dU);

    vec3 normal = normalize(cross(pR - pL, pU - pD));
    return dot(normal, n) < 0.0 ? -normal : normal;
}

void main()
{
    vec2 uv0 = mix(tcTexCoord[0], tcTexCoord[1], gl_TessCoord.x);
    vec2 uv1 = mix(tcTexCoord[3], tcTexCoord[2], gl_TessCoord.x);
    vec2 uv = mix(uv0, uv1, gl_TessCoord.y);
    float skirt0 = mix(tcSkirt[0], tcSkirt[1], gl_TessCoord.x);
    float skirt1 = mix(tcSkirt[3], tcSkirt[2], gl_TessCoord.x);
    float skirtWeight = clamp(mix(skirt0, skirt1, gl_TessCoord.y), 0.0, 1.0);

    teTexCoord = uv;

    vec3 sphereDir = normalize(cubeFacePoint(uv));
    teSphereDir = sphereDir;
    float h = finalTerrainHeightAtUv(uv);
    teSurfaceHeight = h;
    teSkirt = skirtWeight;
    h -= skirtWeight * terrainSkirtDepth / max(heightScale, 0.0001);
    teHeight = h;

    vec3 localPos = sphereDir * (planetRadius + h * heightScale);
    vec4 worldPos = model * vec4(localPos, 1.0);
    teWorldPos = worldPos.xyz;

    vec3 localNormal = computeSphericalNormal(sphereDir);
    teNormal = normalize(mat3(transpose(inverse(model))) * localNormal);

    vec4 relativeWorldPos = vec4(worldPos.xyz - cameraPos, 1.0);
    teClipSpacePos = projection * cameraRelativeView * relativeWorldPos;
    gl_Position = teClipSpacePos;
    gl_ClipDistance[0] = dot(worldPos, clipPlane);
}
