#version 410 core

in vec2 vUv;
out vec4 FragColor;

uniform sampler2D transmittanceTexture;
uniform sampler2D irradianceTexture;
uniform sampler3D previousScatteringTexture;
uniform vec3 rayleighColor;
uniform vec3 mieColor;
uniform float atmosphereDensity;
uniform float rayleighStrength;
uniform float mieStrength;
uniform float mieAnisotropy;
uniform float atmosphereExposure;
uniform float layerIndex;
uniform float layerCount;
uniform float viewMuSize;
uniform float nuSize;
uniform int scatteringOrder;

float saturate(float value)
{
    return clamp(value, 0.0, 1.0);
}

vec3 toneMap(vec3 color)
{
    return vec3(1.0) - exp(-color * atmosphereExposure);
}

float rayleighPhase(float nu)
{
    return 0.75 * (1.0 + nu * nu);
}

float henyeyGreenstein(float nu, float g)
{
    float g2 = g * g;
    float denom = max(0.04, pow(1.0 + g2 - 2.0 * g * nu, 1.5));
    return (1.0 - g2) / denom;
}

void main()
{
    float packedIndex = floor(vUv.x * viewMuSize * nuSize);
    float viewIndex = mod(packedIndex, viewMuSize);
    float nuIndex = floor(packedIndex / viewMuSize);
    float viewMu = (viewIndex + 0.5) / viewMuSize * 2.0 - 1.0;
    float nu = (nuIndex + 0.5) / nuSize * 2.0 - 1.0;
    float height01 = saturate(vUv.y);
    float sunMu = layerCount > 1.0 ? layerIndex / (layerCount - 1.0) * 2.0 - 1.0 : 1.0;

    vec3 transmittance = texture(transmittanceTexture, vec2(sunMu * 0.5 + 0.5, height01)).rgb;
    vec3 irradiance = texture(irradianceTexture, vec2(sunMu * 0.5 + 0.5, height01)).rgb;

    float localSun = smoothstep(-0.22, 0.18, sunMu);
    float twilight = smoothstep(-0.58, 0.10, sunMu) * (1.0 - smoothstep(0.02, 0.68, sunMu));
    float horizonPath = mix(0.32, 3.4, pow(1.0 - abs(viewMu), 1.7));
    float densityR = exp(-height01 * 4.6) * horizonPath * atmosphereDensity;
    float densityM = exp(-height01 * 9.0) * horizonPath * atmosphereDensity;

    vec3 betaR = rayleighColor * rayleighStrength;
    vec3 betaM = mieColor * mieStrength;
    vec3 direct = betaR * rayleighPhase(nu) * densityR * transmittance * (0.18 + localSun * 0.82);
    direct += betaM * henyeyGreenstein(nu, clamp(mieAnisotropy, 0.0, 0.95)) * densityM
            * transmittance * (0.06 + localSun * 0.32 + twilight * 0.26);

    vec3 previous = texture(previousScatteringTexture, vec3(vUv.x, height01, sunMu * 0.5 + 0.5)).rgb;
    float order = float(scatteringOrder);
    float multipleWeight = scatteringOrder <= 1 ? 0.0 : pow(0.58, order - 2.0);
    vec3 multiple = previous * (betaR * densityR * 0.34 + betaM * densityM * 0.075) * multipleWeight;
    multiple += irradiance * (betaR * densityR * 0.090 + betaM * densityM * 0.032) * multipleWeight;
    multiple += betaR * densityR * twilight * multipleWeight * 0.040;

    vec3 radiance = scatteringOrder <= 1 ? direct : multiple;
    FragColor = vec4(toneMap(radiance), 1.0);
}
