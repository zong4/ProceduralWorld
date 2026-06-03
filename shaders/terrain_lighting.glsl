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
    float specular = pow(max(dot(N, H), 0.0), 42.0) * surface.specularStrength;
    float riverSpecular = pow(max(dot(N, H), 0.0), 96.0) * surface.riverSpecular * 0.75;

    vec3 sunColor = vec3(1.08, 1.00, 0.88);
    vec3 groundBounce = vec3(0.13, 0.105, 0.075);

    vec3 ambientLight = mix(groundBounce, skyColor * 0.10, surface.radialAlignment);
    ambientLight += vec3(0.040, 0.035, 0.028);

    vec3 color = surface.baseColor * (ambientLight + ndotl * sunColor) + (specular + riverSpecular) * sunColor;

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
