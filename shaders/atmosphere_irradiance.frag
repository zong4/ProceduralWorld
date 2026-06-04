#version 410 core

in vec2 vUv;
out vec4 FragColor;

uniform sampler2D transmittanceTexture;
uniform vec3 rayleighColor;
uniform vec3 mieColor;
uniform float atmosphereDensity;
uniform float rayleighStrength;
uniform float mieStrength;
uniform float atmosphereExposure;

float saturate(float value)
{
    return clamp(value, 0.0, 1.0);
}

vec3 toneMap(vec3 color)
{
    return vec3(1.0) - exp(-color * atmosphereExposure);
}

void main()
{
    float height01 = saturate(vUv.y);
    float sunMu = vUv.x * 2.0 - 1.0;
    float horizon = 1.0 - smoothstep(-0.12, 0.45, sunMu);
    vec3 transmittance = texture(transmittanceTexture, vec2(sunMu * 0.5 + 0.5, height01)).rgb;

    vec3 betaR = rayleighColor * rayleighStrength;
    vec3 betaM = mieColor * mieStrength;
    float localSun = smoothstep(-0.24, 0.16, sunMu);
    float twilight = smoothstep(-0.55, 0.12, sunMu) * (1.0 - smoothstep(0.05, 0.70, sunMu));
    float density = exp(-height01 * 4.2) * atmosphereDensity;

    vec3 singleBounce = (betaR * 0.16 + betaM * 0.055) * transmittance * localSun * density;
    vec3 skyBounce = betaR * (0.040 + horizon * 0.050 + twilight * 0.080) * density;
    vec3 groundBounce = vec3(0.42, 0.48, 0.54) * (0.010 + twilight * 0.018) * density;

    FragColor = vec4(toneMap(singleBounce + skyBounce + groundBounce), 1.0);
}
