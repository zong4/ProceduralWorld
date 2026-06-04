#version 410 core

in vec2 vUv;
out vec4 FragColor;

uniform vec3 rayleighColor;
uniform vec3 mieColor;
uniform float atmosphereDensity;
uniform float rayleighStrength;
uniform float mieStrength;

void main()
{
    float height01 = clamp(vUv.y, 0.0, 1.0);
    float sunMu = vUv.x * 2.0 - 1.0;
    float horizonPath = mix(2.8, 0.22, smoothstep(-0.08, 0.62, sunMu));
    float density = exp(-height01 * 5.4) * horizonPath * atmosphereDensity;
    vec3 beta = rayleighColor * rayleighStrength * 0.18 + mieColor * mieStrength * 0.10;
    vec3 transmittance = exp(-beta * density);
    FragColor = vec4(transmittance, 1.0);
}
