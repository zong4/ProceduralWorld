#version 410 core

in vec2 vUv;

out vec4 FragColor;

uniform vec3 cameraPos;
uniform vec3 cameraForward;
uniform vec3 cameraRight;
uniform vec3 cameraUp;
uniform vec3 lightDir;
uniform vec3 mieColor;
uniform vec3 cloudColor;
uniform mat4 model;
uniform mat4 inverseViewProjection;
uniform float cameraTanHalfFov;
uniform float cameraAspectRatio;
uniform vec2 framebufferSize;
uniform float planetRadius;
uniform float atmosphereRadius;
uniform float surfaceLimbRadius;
uniform float atmosphereDensity;
uniform float atmosphereExposure;
uniform float scatteringViewMuSize;
uniform float scatteringNuSize;
uniform float scatteringHeight;
uniform float scatteringDepth;
uniform float cloudCoverage;
uniform float cloudSharpness;
uniform float cloudScale;
uniform float cloudSpeed;
uniform float cloudHeight;
uniform float cloudThickness;
uniform float cloudDensity;
uniform float cloudShadowStrength;
uniform float cloudOpacity;
uniform float timeSeconds;
uniform int renderClouds;
uniform int cloudStepCount;
uniform int cloudLightStepCount;
uniform sampler2D atmosphereIrradianceTexture;
uniform sampler3D atmosphereScatteringTexture;
uniform sampler2D sceneDepthTexture;

float saturate(float value)
{
    return clamp(value, 0.0, 1.0);
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

vec3 sampleScatteringLut(float viewMu, float nu, float height01, float sunMu)
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

vec3 toneMap(vec3 color)
{
    return vec3(1.0) - exp(-color * atmosphereExposure);
}

vec3 reconstructWorldPosition(vec2 screenUv, float depth)
{
    vec4 clip = vec4(screenUv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 world = inverseViewProjection * clip;
    return world.xyz / max(abs(world.w), 0.000001);
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

float hash31(vec3 p)
{
    p = fract(p * 0.1031);
    p += dot(p, p.yzx + 33.33);
    return fract((p.x + p.y) * p.z);
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

float fbm(vec3 p)
{
    float value = 0.0;
    float amplitude = 0.52;
    float totalAmplitude = 0.0;
    for (int i = 0; i < 5; ++i) {
        value += valueNoise(p) * amplitude;
        totalAmplitude += amplitude;
        p = p * 2.03 + vec3(11.7, 5.3, 3.1);
        amplitude *= 0.52;
    }
    return value / max(totalAmplitude, 0.001);
}

vec3 hash33(vec3 p)
{
    p = vec3(dot(p, vec3(127.1, 311.7, 74.7)),
             dot(p, vec3(269.5, 183.3, 246.1)),
             dot(p, vec3(113.5, 271.9, 124.6)));
    return fract(sin(p) * 43758.5453);
}

vec2 worleyF1F2(vec3 p)
{
    vec3 cell = floor(p);
    vec3 local = fract(p);
    float f1 = 8.0;
    float f2 = 8.0;
    for (int z = -1; z <= 1; ++z) {
        for (int y = -1; y <= 1; ++y) {
            for (int x = -1; x <= 1; ++x) {
                vec3 offset = vec3(float(x), float(y), float(z));
                vec3 feature = offset + hash33(cell + offset) - local;
                float distanceSq = dot(feature, feature);
                if (distanceSq < f1) {
                    f2 = f1;
                    f1 = distanceSq;
                } else if (distanceSq < f2) {
                    f2 = distanceSq;
                }
            }
        }
    }
    return sqrt(vec2(f1, f2));
}

vec3 cloudLocalPosition(vec3 worldPos)
{
    return transpose(mat3(model)) * worldPos;
}

vec3 cloudNoiseDomain(vec3 p)
{
    mat3 basis = mat3(0.36, 0.82, 0.44,
                      -0.74, 0.52, -0.43,
                      -0.57, -0.18, 0.80);
    vec3 q = basis * p;
    return vec3(q.x + q.y * 0.23,
                q.y + q.z * 0.19,
                q.z + q.x * 0.17);
}

float cloudDensityAt(vec3 localPosition, float layer01)
{
    vec3 wind = vec3(0.73, 0.18, -0.42) * (timeSeconds * cloudSpeed);
    float scale = max(cloudScale, 0.01) / max(planetRadius, 1.0);
    vec3 baseP = cloudNoiseDomain(localPosition * scale) + wind;
    vec3 warp = vec3(fbm(baseP * 1.17 + vec3(17.0, 3.0, 11.0)),
                     fbm(baseP * 1.09 + vec3(5.0, 23.0, 7.0)),
                     fbm(baseP * 1.21 + vec3(31.0, 13.0, 2.0))) - vec3(0.5);
    vec3 p = baseP + cloudNoiseDomain(warp) * 1.18;
    float broad = fbm(p);
    float detail = fbm(p * 2.15 + vec3(17.0, 4.0, 9.0));
    float wisps = fbm(p * 4.8 + vec3(3.0, 19.0, 11.0));
    vec2 cell = worleyF1F2(p * 1.35 + vec3(2.0, 13.0, 7.0));
    float billow = 1.0 - saturate(cell.x * 1.05);
    float cellRidge = saturate((cell.y - cell.x) * 1.05);
    float cloudMass = broad * 0.74 + billow * 0.16 + cellRidge * 0.10;
    float threshold = mix(0.76, 0.28, saturate(cloudCoverage));
    float width = mix(0.40, 0.13, saturate(cloudSharpness / 3.0));
    float density = smoothstep(threshold, threshold + width, cloudMass);

    float flatBase = smoothstep(0.02, 0.10, layer01);
    float coreBulge = smoothstep(0.06, 0.25, layer01) * (1.0 - smoothstep(0.78, 1.0, layer01));
    float topSoft = 1.0 - smoothstep(0.88, 1.0, layer01);
    float profile = flatBase * max(topSoft * 0.98, coreBulge * 1.35);

    float edgeErosion = smoothstep(0.18, 0.92, detail * 0.70 + wisps * 0.30);
    float topErosion = smoothstep(0.48, 1.0, layer01);
    density -= edgeErosion * (0.055 + topErosion * 0.10);
    density *= profile;
    density *= mix(1.02, 1.34, coreBulge);
    return pow(saturate(density), max(cloudSharpness * 0.58, 0.01));
}

vec2 localCloudLayerRadii(vec3 localDirection, float cloudBottomRadius, float cloudTopRadius)
{
    float shellThickness = max(cloudTopRadius - cloudBottomRadius, 0.001);
    vec3 wind = vec3(0.73, 0.18, -0.42) * (timeSeconds * cloudSpeed * 0.55);
    vec3 p = cloudNoiseDomain(localDirection * max(cloudScale * 0.72, 0.01)) + wind;
    p += cloudNoiseDomain(localDirection.yzx) * 0.34;
    float tower = fbm(p * 0.90 + vec3(23.0, 4.0, 9.0));
    float scallop = fbm(p * 2.2 + vec3(5.0, 31.0, 12.0));
    float topLift = smoothstep(0.46, 0.88, tower) * (0.30 + scallop * 0.44);
    float topDrop = smoothstep(0.18, 0.48, 1.0 - tower) * (0.10 + (1.0 - scallop) * 0.18);
    float baseLift = smoothstep(0.54, 0.92, scallop) * 0.10;
    float localBottom = cloudBottomRadius + shellThickness * baseLift;
    float nominalTop = cloudBottomRadius + shellThickness * 0.80;
    float localTop = nominalTop + shellThickness * topLift * 0.42 - shellThickness * topDrop * 0.16;
    localTop = min(localTop, min(cloudTopRadius, atmosphereRadius - 0.02));
    localBottom = min(localBottom, localTop - shellThickness * 0.18);
    return vec2(localBottom, localTop);
}

float cloudDensityAtWorld(vec3 worldPos, float cloudBottomRadius, float cloudTopRadius)
{
    float radius = length(worldPos);
    vec3 localPosition = cloudLocalPosition(worldPos);
    vec3 localDirection = normalize(localPosition);
    vec2 localLayer = localCloudLayerRadii(localDirection, cloudBottomRadius, cloudTopRadius);
    if (radius < localLayer.x || radius > localLayer.y) {
        return 0.0;
    }
    float layer01 = saturate((radius - localLayer.x) / max(localLayer.y - localLayer.x, 0.001));
    float density = cloudDensityAt(localPosition, layer01);
    vec3 localPos = cloudNoiseDomain(localPosition);
    vec3 wind = vec3(0.73, 0.18, -0.42) * (timeSeconds * cloudSpeed);
    float volumeBreakup = fbm(localPos * 0.022 + wind * 1.7 + vec3(9.0, 2.0, 17.0));
    float volumeDetail = fbm(localPos * 0.058 + wind * 3.1 + vec3(21.0, 11.0, 4.0));
    vec2 volumeCell = worleyF1F2(localPos * 0.018 + wind + vec3(4.0, 18.0, 29.0));
    float puffyCell = 1.0 - saturate(volumeCell.x * 1.12);
    float cavity = smoothstep(0.20, 0.66, volumeBreakup * 0.55 + volumeDetail * 0.18 + puffyCell * 0.20);
    float shreddedEdge = smoothstep(0.06, 0.34, min(layer01, 1.0 - layer01));
    density *= mix(0.82, 1.26, cavity) * mix(0.82, 1.0, shreddedEdge);
    return saturate(density);
}

vec2 cloudLightOpticalDepth(vec3 worldPos, vec3 sunDir, float cloudBottomRadius, float cloudTopRadius)
{
    vec2 topHit;
    if (!raySphere(worldPos, sunDir, cloudTopRadius, topHit)) {
        return vec2(0.0);
    }

    float rayEnd = max(topHit.y, 0.0);
    if (rayEnd <= 0.001) {
        return vec2(0.0);
    }

    int lightSteps = clamp(cloudLightStepCount, 1, 8);
    float stepLength = rayEnd / float(lightSteps);
    float opticalDepth = 0.0;
    for (int i = 0; i < 8; ++i) {
        if (i >= lightSteps) {
            break;
        }
        float t = (float(i) + 0.5) * stepLength;
        vec3 samplePos = worldPos + sunDir * t;
        opticalDepth += cloudDensityAtWorld(samplePos, cloudBottomRadius, cloudTopRadius) * stepLength;
    }

    float thickness = max(cloudTopRadius - cloudBottomRadius, 0.001);
    return vec2(opticalDepth / thickness * cloudDensity, thickness);
}

bool cloudShellInterval(vec3 origin,
                        vec3 direction,
                        float cloudBottomRadius,
                        float cloudTopRadius,
                        float segmentStart,
                        float segmentEnd,
                        out vec2 interval)
{
    vec2 topHit;
    if (!raySphere(origin, direction, cloudTopRadius, topHit)) {
        return false;
    }

    float startT = max(segmentStart, topHit.x);
    float endT = min(segmentEnd, topHit.y);
    if (endT <= startT) {
        return false;
    }

    vec2 bottomHit;
    if (raySphere(origin, direction, cloudBottomRadius, bottomHit)) {
        if (bottomHit.x <= startT && bottomHit.y > startT) {
            startT = min(bottomHit.y, endT);
        } else if (bottomHit.x > startT && bottomHit.x < endT) {
            endT = bottomHit.x;
        }
    }

    if (endT <= startT) {
        return false;
    }

    interval = vec2(startT, endT);
    return true;
}

vec4 raymarchCloudVolume(vec3 origin,
                         vec3 direction,
                         vec3 sunDir,
                         float segmentStart,
                         float segmentEnd,
                         float cloudBottomRadius,
                         float cloudTopRadius,
                         float viewSunMu)
{
    int steps = clamp(cloudStepCount, 4, 32);
    float stepLength = (segmentEnd - segmentStart) / float(steps);
    if (stepLength <= 0.001) {
        return vec4(0.0);
    }

    float transmittance = 1.0;
    vec3 accumColor = vec3(0.0);
    float shellThickness = max(cloudTopRadius - cloudBottomRadius, 0.001);
    float tangentPathBoost = saturate((segmentEnd - segmentStart) / (shellThickness * 3.8));
    for (int i = 0; i < 32; ++i) {
        if (i >= steps || transmittance <= 0.015) {
            break;
        }

        float jitter = hash31(vec3(gl_FragCoord.xy, float(i) * 17.0));
        float t = segmentStart + (float(i) + mix(0.35, 0.65, jitter)) * stepLength;
        vec3 samplePos = origin + direction * t;
        float density = cloudDensityAtWorld(samplePos, cloudBottomRadius, cloudTopRadius);
        if (density <= 0.001) {
            continue;
        }

        float localSun = smoothstep(-0.28, 0.24, dot(normalize(samplePos), sunDir));
        vec2 lightDepth = cloudLightOpticalDepth(samplePos, sunDir, cloudBottomRadius, cloudTopRadius);
        float lightOpticalDepth = lightDepth.x;
        float beer = exp(-lightOpticalDepth * cloudShadowStrength * 2.65);
        float powder = 1.0 - exp(-lightOpticalDepth * 2.20);
        float lightTransmittance = clamp(beer + powder * 0.50 * (1.0 - beer), 0.0, 1.0);
        float silverLining = pow(saturate(viewSunMu), 18.0) * (0.28 + 0.72 * localSun);
        vec3 warmEdge = mieColor * (0.10 + silverLining * 0.74);
        float selfShadow = exp(-density * cloudDensity * 0.92);
        vec3 ambientScatter = cloudColor * vec3(0.30, 0.36, 0.50) * (0.36 + tangentPathBoost * 0.16);
        vec3 cloudShadow = ambientScatter * mix(0.62, 1.0, selfShadow);
        vec3 cloudLight = cloudColor * (0.62 + localSun * 1.02) * (0.64 + powder * 0.34) + warmEdge;
        vec3 cloudLit = mix(cloudShadow, cloudLight, lightTransmittance);
        cloudLit *= mix(0.96, 1.08, tangentPathBoost);

        float volumeDensity = density * cloudDensity * mix(1.0, 1.16, tangentPathBoost);
        float stepAlpha = 1.0 - exp(-volumeDensity * cloudOpacity * stepLength / shellThickness * 5.20);
        accumColor += transmittance * stepAlpha * cloudLit;
        transmittance *= 1.0 - stepAlpha;
    }

    return vec4(accumColor, 1.0 - transmittance);
}

void main()
{
    vec2 screenUv = gl_FragCoord.xy / max(framebufferSize, vec2(1.0));
    vec2 ndc = screenUv * 2.0 - 1.0;
    vec3 rayDir = normalize(cameraForward
                          + cameraRight * (ndc.x * cameraTanHalfFov * cameraAspectRatio)
                          + cameraUp * (ndc.y * cameraTanHalfFov));
    vec3 sunDir = normalize(-lightDir);

    vec2 atmosphereHit;
    if (!raySphere(cameraPos, rayDir, atmosphereRadius, atmosphereHit)) {
        discard;
    }

    float shellThickness = max(atmosphereRadius - planetRadius, 0.001);
    float rayStart = max(atmosphereHit.x, 0.0);
    float rayEnd = atmosphereHit.y;
    bool hasSceneSurface = false;
    float sceneSurfaceT = rayEnd;

    float sceneDepth = texture(sceneDepthTexture, screenUv).r;
    if (sceneDepth < 0.999999) {
        vec3 sceneWorldPos = reconstructWorldPosition(screenUv, sceneDepth);
        vec3 cameraToScene = sceneWorldPos - cameraPos;
        float projectedT = dot(cameraToScene, rayDir);
        float offRayError = length(cameraToScene - rayDir * projectedT);
        if (projectedT > rayStart && offRayError < max(projectedT * 0.01, 0.25)) {
            hasSceneSurface = true;
            sceneSurfaceT = projectedT;
            rayEnd = min(rayEnd, sceneSurfaceT);
        }
    }

    float surfaceRadius = clamp(surfaceLimbRadius,
                                planetRadius,
                                atmosphereRadius - shellThickness * 0.20);
    vec2 surfaceHit;
    bool hitsSphereSurface = raySphere(cameraPos, rayDir, surfaceRadius, surfaceHit) && surfaceHit.x > rayStart;
    if (!hasSceneSurface && hitsSphereSurface) {
        rayEnd = min(rayEnd, surfaceHit.x);
    }
    bool hitsSurface = hasSceneSurface || hitsSphereSurface;

    float rayLength = max(rayEnd - rayStart, 0.0);
    if (rayLength <= 0.0001) {
        discard;
    }

    float viewProjection = dot(cameraPos, rayDir);
    float impactRadius = sqrt(max(dot(cameraPos, cameraPos) - viewProjection * viewProjection, 0.0));
    float viewSunMu = dot(rayDir, sunDir);
    float sunFacing = smoothstep(-0.32, 0.64, viewSunMu);
    float twilight = smoothstep(-0.62, 0.18, viewSunMu) * (1.0 - smoothstep(0.02, 0.78, viewSunMu));

    float signedShell01 = (impactRadius - surfaceRadius) / shellThickness;
    float surfaceShell01 = saturate(signedShell01);
    float tangentWeight = exp(-abs(signedShell01) * 2.65);
    float surfaceContact = 1.0 - smoothstep(0.90, 1.0, surfaceShell01);
    float outerFade = 1.0 - smoothstep(0.78, 1.0, surfaceShell01);
    float normalizedPath = saturate(rayLength / (shellThickness * 3.2));
    float atmosphereColumn = saturate(max(tangentWeight * surfaceContact, normalizedPath * 0.34));
    vec3 samplePos = cameraPos + rayDir * (rayStart + rayLength * 0.42);
    vec3 sampleNormal = normalize(samplePos);
    float height01 = saturate((length(samplePos) - surfaceRadius) / shellThickness);
    float sunMu = dot(sampleNormal, sunDir);
    float viewMu = dot(sampleNormal, rayDir);
    vec3 scattering = sampleScatteringLut(viewMu, viewSunMu, height01, sunMu);
    vec3 irradiance = texture(atmosphereIrradianceTexture,
                              vec2(sunMu * 0.5 + 0.5, height01)).rgb;

    float surfaceAir = hitsSurface ? pow(normalizedPath, 0.78) * 0.052 : 0.0;
    float scatteringLuma = max(max(scattering.r, scattering.g), scattering.b);
    float geometryAir = tangentWeight * outerFade * 0.080;
    float shellAir = atmosphereColumn * (0.034 + scatteringLuma * 0.58) + geometryAir;
    if (hasSceneSurface) {
        float surfaceFade = smoothstep(0.0, shellThickness * 1.8, rayLength);
        float nearSurfaceTangent = smoothstep(0.82, 1.05, signedShell01);
        shellAir *= mix(0.18, 0.58, nearSurfaceTangent) * surfaceFade;
        geometryAir *= 0.25;
    }
    float alpha = saturate((surfaceAir + shellAir) * outerFade * atmosphereDensity);

    vec3 lowAir = vec3(0.22, 0.43, 0.86) * (0.055 + sunFacing * 0.12);
    vec3 horizonAir = scattering * (0.82 + sunFacing * 0.52)
                    + irradiance * (0.16 + twilight * 0.20)
                    + mieColor * twilight * 0.040;
    vec3 color = mix(lowAir, horizonAir, saturate(atmosphereColumn + normalizedPath * 0.32));
    color = toneMap(color * (0.90 + atmosphereDensity * 0.38));

    if (renderClouds != 0 && cloudOpacity > 0.001) {
        float requestedThickness = clamp(max(cloudThickness, 0.05), 0.05, max(shellThickness - 0.10, 0.05));
        float requestedBottom = planetRadius + clamp(cloudHeight, 0.5, max(shellThickness - 0.5, 0.5));
        float cloudTopRadius = min(atmosphereRadius - 0.05, requestedBottom + requestedThickness * 1.45);
        float cloudBottomRadius = max(planetRadius + 0.25, cloudTopRadius - requestedThickness * 1.45);
        vec2 cloudInterval;
        if (cloudTopRadius > cloudBottomRadius + 0.01
            && cloudShellInterval(cameraPos, rayDir, cloudBottomRadius, cloudTopRadius, rayStart, rayEnd, cloudInterval)) {
            vec4 cloudVolume = raymarchCloudVolume(cameraPos,
                                                   rayDir,
                                                   sunDir,
                                                   cloudInterval.x,
                                                   cloudInterval.y,
                                                   cloudBottomRadius,
                                                   cloudTopRadius,
                                                   viewSunMu);
            float cloudAlpha = saturate(cloudVolume.a);
            color = color * (1.0 - cloudAlpha * 0.86) + cloudVolume.rgb;
            alpha = saturate(alpha + cloudAlpha * (1.0 - alpha * 0.22));
        }
    }

    FragColor = vec4(color * alpha, alpha);
}
