uniform vec3 skyColor;
uniform float fogDensity;
uniform int renderAtmosphere;
uniform float atmosphereRadius;
uniform float atmosphereDensity;
uniform float atmosphereExposure;
uniform float scatteringViewMuSize;
uniform float scatteringNuSize;
uniform float scatteringHeight;
uniform float scatteringDepth;
uniform vec3 mieColor;
uniform sampler2D atmosphereIrradianceTexture;
uniform sampler3D atmosphereScatteringTexture;

float lightingSaturate(float value)
{
    return clamp(value, 0.0, 1.0);
}

bool lightingRaySphere(vec3 origin, vec3 direction, float radius, out vec2 hit)
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

float lightingScatteringTexelU(float viewIndex, float nuIndex)
{
    return (nuIndex * scatteringViewMuSize + viewIndex + 0.5)
         / max(scatteringViewMuSize * scatteringNuSize, 1.0);
}

vec3 lightingScatteringTexel(float viewIndex, float nuIndex, float heightIndex, float sunIndex)
{
    return texture(atmosphereScatteringTexture,
                   vec3(lightingScatteringTexelU(viewIndex, nuIndex),
                        (heightIndex + 0.5) / max(scatteringHeight, 1.0),
                        (sunIndex + 0.5) / max(scatteringDepth, 1.0))).rgb;
}

vec3 lightingSampleScatteringLut(float viewMu, float nu, float height01, float sunMu)
{
    float viewCoord = lightingSaturate(viewMu * 0.5 + 0.5) * (scatteringViewMuSize - 1.0);
    float nuCoord = lightingSaturate(nu * 0.5 + 0.5) * (scatteringNuSize - 1.0);
    float heightCoord = lightingSaturate(height01) * (scatteringHeight - 1.0);
    float sunCoord = lightingSaturate(sunMu * 0.5 + 0.5) * (scatteringDepth - 1.0);

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

    vec3 h000 = mix(lightingScatteringTexel(view0, nu0, height0, sun0),
                    lightingScatteringTexel(view1, nu0, height0, sun0),
                    viewMix);
    vec3 h010 = mix(lightingScatteringTexel(view0, nu1, height0, sun0),
                    lightingScatteringTexel(view1, nu1, height0, sun0),
                    viewMix);
    vec3 h100 = mix(lightingScatteringTexel(view0, nu0, height1, sun0),
                    lightingScatteringTexel(view1, nu0, height1, sun0),
                    viewMix);
    vec3 h110 = mix(lightingScatteringTexel(view0, nu1, height1, sun0),
                    lightingScatteringTexel(view1, nu1, height1, sun0),
                    viewMix);
    vec3 s0 = mix(mix(h000, h010, nuMix),
                  mix(h100, h110, nuMix),
                  heightMix);

    vec3 h001 = mix(lightingScatteringTexel(view0, nu0, height0, sun1),
                    lightingScatteringTexel(view1, nu0, height0, sun1),
                    viewMix);
    vec3 h011 = mix(lightingScatteringTexel(view0, nu1, height0, sun1),
                    lightingScatteringTexel(view1, nu1, height0, sun1),
                    viewMix);
    vec3 h101 = mix(lightingScatteringTexel(view0, nu0, height1, sun1),
                    lightingScatteringTexel(view1, nu0, height1, sun1),
                    viewMix);
    vec3 h111 = mix(lightingScatteringTexel(view0, nu1, height1, sun1),
                    lightingScatteringTexel(view1, nu1, height1, sun1),
                    viewMix);
    vec3 s1 = mix(mix(h001, h011, nuMix),
                  mix(h101, h111, nuMix),
                  heightMix);

    return mix(s0, s1, sunMix);
}

vec3 applyAerialPerspective(vec3 surfaceColor, vec3 worldPos)
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
    if (!lightingRaySphere(cameraPos, rayDir, atmosphereRadius, atmosphereHit)) {
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
    float height01 = lightingSaturate((length(midPos) - planetRadius) / shellThickness);
    float viewMu = dot(midNormal, rayDir);
    float viewSunMu = dot(rayDir, sunDir);
    float sunMu = dot(midNormal, sunDir);

    float viewProjection = dot(cameraPos, rayDir);
    float impactRadius = sqrt(max(dot(cameraPos, cameraPos) - viewProjection * viewProjection, 0.0));
    float tangent01 = 1.0 - smoothstep(0.0, shellThickness * 2.6, abs(impactRadius - planetRadius));
    float path01 = lightingSaturate(rayLength / (shellThickness * 7.5));
    float densityHeight = exp(-height01 * 3.8);
    float airAmount = lightingSaturate((path01 * 0.55 + tangent01 * 0.35) * densityHeight * atmosphereDensity);

    vec3 scattering = lightingSampleScatteringLut(viewMu, viewSunMu, height01, sunMu);
    vec3 irradiance = texture(atmosphereIrradianceTexture, vec2(sunMu * 0.5 + 0.5, height01)).rgb;
    vec3 blueAir = vec3(0.22, 0.42, 0.84) * (0.06 + airAmount * 0.34);
    vec3 sunAir = scattering * (0.30 + lightingSaturate(viewSunMu * 0.5 + 0.5) * 0.55)
                + irradiance * 0.045
                + mieColor * pow(lightingSaturate(viewSunMu), 8.0) * 0.018;
    vec3 inScattering = (blueAir + sunAir) * airAmount * atmosphereExposure;

    vec3 extinction = vec3(0.16, 0.28, 0.58) * airAmount;
    vec3 transmittance = exp(-extinction);
    return surfaceColor * transmittance + inScattering;
}

// 简化 Blinn-Phong + 半球环境光 + 指数雾。
LightingData evaluateLighting(SurfaceData surface, vec3 worldPos, vec3 shadingNormal)
{
    LightingData lighting;

    vec3 N = normalize(surface.detailNormal);
    vec3 L = normalize(-lightDir);
    vec3 V = normalize(cameraPos - worldPos);
    vec3 H = normalize(L + V);

    float ndotl = max(dot(N, L), 0.0);
    float gloss = mix(96.0, 18.0, clamp(surface.roughness, 0.0, 1.0));
    float specular = pow(max(dot(N, H), 0.0), gloss) * surface.specularStrength;
    float riverSpecular = pow(max(dot(N, H), 0.0), 96.0) * surface.riverSpecular * 0.75;

    vec3 sunColor = vec3(1.04, 1.00, 0.96);
    vec3 groundBounce = vec3(0.12, 0.105, 0.082);

    vec3 ambientLight = mix(groundBounce, skyColor * 0.055, surface.radialAlignment);
    ambientLight += vec3(0.044, 0.040, 0.034);

    vec3 color = surface.baseColor * (ambientLight + ndotl * sunColor) + (specular + riverSpecular) * sunColor;

    // 使用距离平方指数雾，将远处地形推向 skyColor。
    float fogDistance = length(cameraPos - worldPos);
    lighting.fogFactor = exp(-fogDistance * fogDistance * fogDensity * 0.58);
    lighting.litColor = applyAerialPerspective(mix(skyColor, color, lighting.fogFactor), worldPos);
    return lighting;
}

vec3 toneMapAndGamma(vec3 color)
{
    // Reinhard tone mapping 后转到近似 sRGB。
    color = color / (color + vec3(1.0));
    return pow(color, vec3(1.0 / 2.2));
}
