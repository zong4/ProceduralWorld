#version 410 core

in vec3 vWorldPos;
in vec3 vSphereNormal;

out vec4 FragColor;

uniform vec3 cameraPos;
uniform vec3 lightDir;
uniform vec3 rayleighColor;
uniform vec3 mieColor;
uniform vec3 cloudColor;
uniform mat4 model;
uniform float planetRadius;
uniform float atmosphereRadius;
uniform float atmosphereDensity;
uniform float rayleighStrength;
uniform float mieStrength;
uniform float mieAnisotropy;
uniform float atmosphereExposure;
uniform float cloudCoverage;
uniform float cloudSharpness;
uniform float cloudScale;
uniform float cloudSpeed;
uniform float cloudRotationSpeed;
uniform float cloudHeight;
uniform float cloudOpacity;
uniform float timeSeconds;
uniform int renderClouds;

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
    float g2 = g * g;
    float denom = max(0.04, pow(1.0 + g2 - 2.0 * g * cosTheta, 1.5));
    return (1.0 - g2) / denom;
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

vec3 rotateAroundAxis(vec3 value, vec3 axis, float angleRadians)
{
    float s = sin(angleRadians);
    float c = cos(angleRadians);
    return value * c + cross(axis, value) * s + axis * dot(axis, value) * (1.0 - c);
}

float cloudDensityAt(vec3 direction)
{
    vec3 wind = vec3(0.73, 0.18, -0.42) * (timeSeconds * cloudSpeed);
    vec3 p = direction * max(cloudScale, 0.01) + wind;
    float broad = fbm(p);
    float detail = fbm(p * 3.1 + vec3(17.0, 4.0, 9.0));
    float noiseValue = broad * 0.78 + detail * 0.22;
    float threshold = mix(0.82, 0.22, saturate(cloudCoverage));
    float width = mix(0.26, 0.055, saturate(cloudSharpness / 3.0));
    float density = smoothstep(threshold, threshold + width, noiseValue);
    return pow(saturate(density), max(cloudSharpness, 0.01));
}

void main()
{
    vec3 viewToAtmosphere = normalize(vWorldPos - cameraPos);
    vec3 sunDir = normalize(-lightDir);

    vec2 atmosphereHit;
    if (!raySphere(cameraPos, viewToAtmosphere, atmosphereRadius, atmosphereHit)) {
        discard;
    }

    float rayStart = max(atmosphereHit.x, 0.0);
    float rayEnd = atmosphereHit.y;

    vec2 planetHit;
    bool hitsPlanet = raySphere(cameraPos, viewToAtmosphere, planetRadius, planetHit) && planetHit.x > 0.0;
    if (hitsPlanet) {
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

    if (renderClouds != 0 && cloudOpacity > 0.001) {
        float cloudRadius = planetRadius + clamp(cloudHeight, 0.5, max(shellThickness - 0.5, 0.5));
        vec2 cloudHit;
        if (raySphere(cameraPos, viewToAtmosphere, cloudRadius, cloudHit)) {
            float cloudT = cloudHit.x > rayStart ? cloudHit.x : cloudHit.y;
            if (cloudT >= rayStart && cloudT <= rayEnd) {
                vec3 cloudPos = cameraPos + viewToAtmosphere * cloudT;
                vec3 cloudNormal = normalize(cloudPos);
                vec3 cloudLocalNormal = normalize(transpose(mat3(model)) * cloudNormal);
                float spinRadians = radians(timeSeconds * cloudRotationSpeed);
                cloudLocalNormal = rotateAroundAxis(cloudLocalNormal, vec3(0.0, 1.0, 0.0), spinRadians);
                float density = cloudDensityAt(cloudLocalNormal);
                float ndotSun = dot(cloudNormal, sunDir);
                float localSun = smoothstep(-0.26, 0.20, ndotSun);
                float horizonFade = smoothstep(0.0, 0.18, abs(dot(cloudNormal, viewToAtmosphere)));
                float silverLining = pow(saturate(mu), 18.0) * (0.35 + 0.65 * localSun);
                vec3 warmEdge = mieColor * (0.18 + silverLining * 0.85);
                vec3 cloudLit = cloudColor * (0.36 + localSun * 0.92) + warmEdge;
                float cloudAlpha = density * cloudOpacity * horizonFade;
                color = mix(color, cloudLit, saturate(cloudAlpha));
                alpha = saturate(alpha + cloudAlpha * 0.78);
            }
        }
    }

    FragColor = vec4(color, alpha);
}
