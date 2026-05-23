uniform vec3 skyColor;
uniform float fogDensity;

// 简化 Blinn-Phong + 半球环境光 + 指数雾。
LightingData evaluateLighting(SurfaceData surface, vec3 worldPos, vec3 shadingNormal)
{
    LightingData lighting;

    vec3 N = normalize(shadingNormal);
    vec3 L = normalize(-lightDir);
    vec3 V = normalize(cameraPos - worldPos);
    vec3 H = normalize(L + V);

    float ndotl = max(dot(N, L), 0.0);
    float specular = pow(max(dot(N, H), 0.0), 32.0) * 0.08;

    vec3 sunColor = vec3(1.0, 0.95, 0.85);
    vec3 groundBounce = vec3(0.10, 0.08, 0.06);

    vec3 ambientLight = mix(groundBounce, skyColor * 0.24, surface.radialAlignment);
    ambientLight += vec3(0.035);

    vec3 color = surface.baseColor * (ambientLight + ndotl * sunColor) + specular * sunColor;

    // 使用距离平方指数雾，将远处地形推向 skyColor。
    float fogDistance = length(cameraPos - worldPos);
    lighting.fogFactor = exp(-fogDistance * fogDistance * fogDensity);
    lighting.litColor = mix(skyColor, color, lighting.fogFactor);
    return lighting;
}

vec3 toneMapAndGamma(vec3 color)
{
    // Reinhard tone mapping 后转到近似 sRGB。
    color = color / (color + vec3(1.0));
    return pow(color, vec3(1.0 / 2.2));
}
