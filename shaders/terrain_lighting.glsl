uniform vec3 skyColor;
uniform float fogDensity;

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

    vec3 sunColor = vec3(1.08, 1.00, 0.88);
    vec3 groundBounce = vec3(0.15, 0.115, 0.072);

    vec3 ambientLight = mix(groundBounce, skyColor * 0.055, surface.radialAlignment);
    ambientLight += vec3(0.050, 0.040, 0.027);

    vec3 color = surface.baseColor * (ambientLight + ndotl * sunColor) + (specular + riverSpecular) * sunColor;

    // 使用距离平方指数雾，将远处地形推向 skyColor。
    float fogDistance = length(cameraPos - worldPos);
    lighting.fogFactor = exp(-fogDistance * fogDistance * fogDensity * 0.58);
    lighting.litColor = mix(skyColor, color, lighting.fogFactor);
    return lighting;
}

vec3 toneMapAndGamma(vec3 color)
{
    // Reinhard tone mapping 后转到近似 sRGB。
    color = color / (color + vec3(1.0));
    return pow(color, vec3(1.0 / 2.2));
}
