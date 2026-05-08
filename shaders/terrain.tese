#version 410 core

layout(quads, fractional_even_spacing, ccw) in;

in vec2 tcTexCoord[];

out vec3 teWorldPos;
out vec3 teNormal;
out vec2 teTexCoord;
out float teHeight;
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
uniform float noiseScale;
uniform float mountainMaskStrength;
uniform float mountainMaskScale;
uniform float mountainRidgeSharpness;
uniform float seaLevelOffset;
uniform float terrainShoreLift;
uniform float regionalDetailStrength;
uniform float microDetailStrength;
uniform float regionalDetailStartAltitude;
uniform float regionalDetailEndAltitude;
uniform float microDetailStartAltitude;
uniform float microDetailEndAltitude;
uniform float cameraAltitude;
uniform vec4 clipPlane;
uniform int faceIndex;
uniform float proceduralDataTexelSize;
uniform sampler2DArray proceduralHeightTexture;
uniform sampler2DArray proceduralShoreMaskTexture;

vec3 cubeFacePoint(vec2 uv)
{
    vec2 faceUV = uv * 2.0 - 1.0;
    return faceNormal + faceUV.x * faceAxisU + faceUV.y * faceAxisV;
}

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

float terrainHeightAtUv(vec2 uv)
{
    vec2 clampedUv = clamp(uv, vec2(0.0), vec2(1.0));
    return texture(proceduralHeightTexture, vec3(clampedUv, float(faceIndex))).r;
}

float shoreMaskAtUv(vec2 uv)
{
    vec2 clampedUv = clamp(uv, vec2(0.0), vec2(1.0));
    return clamp(texture(proceduralShoreMaskTexture, vec3(clampedUv, float(faceIndex))).r, 0.0, 1.0);
}

float runtimeTerrainDetail(vec3 sphereDir, float baseHeight)
{
    float regionalWeight = regionalDetailStrength * (1.0 - smoothstep(
        regionalDetailStartAltitude,
        regionalDetailEndAltitude,
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

    float continentalShelf = smoothstep(-0.30, 0.38, baseHeight);
    vec3 mountainP = p * mountainMaskScale + warp * 1.35;
    float mountainBands = 1.0 - abs(gradientLikeNoise(mountainP));
    mountainBands = pow(clamp(mountainBands, 0.0, 1.0), 1.85);
    float mountainField = fbm3(mountainP * 0.72 + vec3(11.7, 2.3, 6.1));
    float mountainMask = smoothstep(0.44, 0.78, mountainBands * 0.68 + mountainField * 0.32);
    mountainMask = pow(clamp(mountainMask * continentalShelf, 0.0, 1.0), 1.15);

    float ridges = 1.0 - abs(gradientLikeNoise(p * 7.0 + warp * 3.4));
    ridges = pow(clamp(ridges, 0.0, 1.0), mountainRidgeSharpness);
    float regional = fbm3(p * 4.2 + warp * 2.5) * 2.0 - 1.0;
    float rollingDetail = fbm3(p * 2.2 + warp * 1.6) * 2.0 - 1.0;
    float micro = fbm3(p * 10.0 + warp * 4.0) * 2.0 - 1.0;

    float detail = 0.0;
    detail += regionalWeight * rollingDetail * 0.08 * (1.0 - mountainMask * 0.45);
    detail += regionalWeight * mountainMaskStrength * mountainMask * (ridges * 0.26 + regional * 0.10);
    detail += microWeight * (micro * 0.055 + ridges * mountainMask * 0.055);
    return detail;
}

float finalTerrainHeightAtUv(vec2 uv)
{
    vec3 sphereDir = normalize(cubeFacePoint(uv));
    float baseHeight = terrainHeightAtUv(uv);
    float h = baseHeight + runtimeTerrainDetail(sphereDir, baseHeight);
    float landMask = smoothstep(seaLevelOffset, seaLevelOffset + 0.025, h);
    h += shoreMaskAtUv(uv) * landMask * terrainShoreLift;
    return h;
}

vec3 displacedPositionFromUv(vec2 uv)
{
    vec3 sphereDir = normalize(cubeFacePoint(uv));
    float h = finalTerrainHeightAtUv(uv);
    return sphereDir * (planetRadius + h * heightScale);
}

vec3 computeNormal(vec2 uv)
{
    float eps = max(proceduralDataTexelSize, 0.0035);
    vec2 uvL = clamp(uv - vec2(eps, 0.0), vec2(0.0), vec2(1.0));
    vec2 uvR = clamp(uv + vec2(eps, 0.0), vec2(0.0), vec2(1.0));
    vec2 uvD = clamp(uv - vec2(0.0, eps), vec2(0.0), vec2(1.0));
    vec2 uvU = clamp(uv + vec2(0.0, eps), vec2(0.0), vec2(1.0));

    vec3 pL = displacedPositionFromUv(uvL);
    vec3 pR = displacedPositionFromUv(uvR);
    vec3 pD = displacedPositionFromUv(uvD);
    vec3 pU = displacedPositionFromUv(uvU);

    return normalize(cross(pR - pL, pU - pD));
}

void main()
{
    vec2 uv0 = mix(tcTexCoord[0], tcTexCoord[1], gl_TessCoord.x);
    vec2 uv1 = mix(tcTexCoord[3], tcTexCoord[2], gl_TessCoord.x);
    vec2 uv = mix(uv0, uv1, gl_TessCoord.y);

    teTexCoord = uv;

    vec3 sphereDir = normalize(cubeFacePoint(uv));
    float h = finalTerrainHeightAtUv(uv);
    teHeight = h;

    vec3 localPos = sphereDir * (planetRadius + h * heightScale);
    vec4 worldPos = model * vec4(localPos, 1.0);
    teWorldPos = worldPos.xyz;

    vec3 localNormal = computeNormal(uv);
    teNormal = normalize(mat3(transpose(inverse(model))) * localNormal);

    vec4 relativeWorldPos = vec4(worldPos.xyz - cameraPos, 1.0);
    teClipSpacePos = projection * cameraRelativeView * relativeWorldPos;
    gl_Position = teClipSpacePos;
    gl_ClipDistance[0] = dot(worldPos, clipPlane);
}
