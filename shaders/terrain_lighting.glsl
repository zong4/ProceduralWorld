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
    vec3 skyColor = vec3(0.53, 0.73, 0.94);
    vec3 groundBounce = vec3(0.10, 0.08, 0.06);

    vec3 ambientLight = mix(groundBounce, skyColor * 0.35, surface.radialAlignment);
    ambientLight += vec3(0.06);

    vec3 color = surface.baseColor * (ambientLight + ndotl * sunColor) + specular * sunColor;

    float fogDistance = length(cameraPos - worldPos);
    lighting.fogFactor = exp(-fogDistance * fogDistance * 0.000015);
    lighting.litColor = mix(skyColor, color, lighting.fogFactor);
    return lighting;
}

vec3 toneMapAndGamma(vec3 color)
{
    color = color / (color + vec3(1.0));
    return pow(color, vec3(1.0 / 2.2));
}
