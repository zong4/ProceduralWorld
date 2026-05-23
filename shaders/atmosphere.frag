#version 410 core

// 简化实时大气散射。
// 沿视线在大气壳内积分 Rayleigh/Mie 密度，并根据太阳方向估计散射颜色。

in vec3 vWorldPos;
in vec3 vSphereNormal;

out vec4 FragColor;

uniform vec3 cameraPos;
uniform vec3 lightDir;
uniform vec3 rayleighColor;
uniform vec3 mieColor;
uniform float planetRadius;
uniform float atmosphereRadius;
uniform float atmosphereDensity;
uniform float rayleighStrength;
uniform float mieStrength;
uniform float mieAnisotropy;
uniform float atmosphereExposure;

float saturate(float value)
{
    return clamp(value, 0.0, 1.0);
}

vec3 toneMap(vec3 color)
{
    return vec3(1.0) - exp(-color * atmosphereExposure);
}

bool raySphere(vec3 origin, vec3 direction, float radius, out vec2 hit)
{
    // 射线和以原点为中心的球求交，返回近/远 t。
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

float henyeyGreenstein(float cosTheta, float g)
{
    // Mie 相函数，g 越大前向散射越强。
    float g2 = g * g;
    float denom = max(0.04, pow(1.0 + g2 - 2.0 * g * cosTheta, 1.5));
    return (1.0 - g2) / denom;
}

void main()
{
    vec3 viewToAtmosphere = normalize(vWorldPos - cameraPos);
    vec3 sunDir = normalize(-lightDir);

    // 先找到视线穿过大气壳的区间。
    vec2 atmosphereHit;
    if (!raySphere(cameraPos, viewToAtmosphere, atmosphereRadius, atmosphereHit)) {
        discard;
    }

    float rayStart = max(atmosphereHit.x, 0.0);
    float rayEnd = atmosphereHit.y;

    vec2 planetHit;
    if (raySphere(cameraPos, viewToAtmosphere, planetRadius, planetHit) && planetHit.x > 0.0) {
        // 如果视线先打到地表，则积分到地表为止。
        rayEnd = min(rayEnd, planetHit.x);
    }

    if (rayEnd <= rayStart) {
        discard;
    }

    float mu = dot(viewToAtmosphere, sunDir);
    float rayleighPhase = 0.75 * (1.0 + mu * mu);
    float miePhase = henyeyGreenstein(mu, clamp(mieAnisotropy, 0.0, 0.95));

    float shellThickness = max(atmosphereRadius - planetRadius, 0.001);
    float rayleighScaleHeight = shellThickness * 0.34;
    float mieScaleHeight = shellThickness * 0.16;
    float rayLength = rayEnd - rayStart;
    float stepLength = rayLength / 18.0;

    vec3 scatteredLight = vec3(0.0);
    float opticalDepth = 0.0;

    for (int i = 0; i < 18; ++i) {
        // 固定步数积分，性能稳定；密度随高度指数衰减。
        float t = rayStart + (float(i) + 0.5) * stepLength;
        vec3 samplePos = cameraPos + viewToAtmosphere * t;
        float altitude = max(length(samplePos) - planetRadius, 0.0);
        vec3 sampleNormal = normalize(samplePos);

        float rayleighDensity = exp(-altitude / rayleighScaleHeight);
        float mieDensity = exp(-altitude / mieScaleHeight);
        float ndotSun = dot(sampleNormal, sunDir);
        float localSun = smoothstep(-0.32, 0.18, ndotSun);
        float terminatorWarmth = smoothstep(-0.24, 0.12, ndotSun)
                               * (1.0 - smoothstep(0.08, 0.50, ndotSun));
        float backscatterFill = 0.030 + terminatorWarmth * 0.075;

        vec2 sunHit;
        raySphere(samplePos, sunDir, atmosphereRadius, sunHit);
        // 简化太阳方向透射：穿过大气越长，直射光越弱。
        float sunPath = max(sunHit.y, 0.0) / shellThickness;
        float sunTransmittance = exp(-sunPath * atmosphereDensity * 0.035);

        float normalizedStep = stepLength / shellThickness;
        vec3 rayleighScatter = rayleighColor * rayleighStrength * rayleighPhase * rayleighDensity;
        vec3 mieScatter = mieColor * mieStrength * miePhase * mieDensity * (0.45 + terminatorWarmth * 1.65);
        vec3 directScatter = (rayleighScatter + mieScatter) * localSun * sunTransmittance;
        vec3 multipleScatterFill = rayleighColor * rayleighStrength * rayleighDensity * backscatterFill;
        scatteredLight += (directScatter + multipleScatterFill) * normalizedStep;
        opticalDepth += (rayleighDensity + mieDensity * 0.35) * normalizedStep;
    }

    vec3 color = toneMap(scatteredLight * atmosphereDensity);
    float alpha = saturate(max(max(color.r, color.g), color.b) * 0.92 + opticalDepth * atmosphereDensity * 0.055);

    FragColor = vec4(color, alpha);
}
