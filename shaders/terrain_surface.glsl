SurfaceData sampleSurfaceData(float height, vec3 worldPos, vec3 shadingNormal)
{
    SurfaceData surface;

    vec3 radialUp = normalize(worldPos);
    surface.radialAlignment = clamp(dot(normalize(shadingNormal), radialUp), 0.0, 1.0);
    surface.slope = 1.0 - surface.radialAlignment;
    surface.height01 = height * 0.5 + 0.5;

    vec3 deepWater    = vec3(0.05, 0.12, 0.28);
    vec3 shallowWater = vec3(0.08, 0.25, 0.45);
    vec3 sand         = vec3(0.76, 0.70, 0.50);
    vec3 grass        = vec3(0.25, 0.50, 0.18);
    vec3 darkGrass    = vec3(0.15, 0.35, 0.10);
    vec3 rock         = vec3(0.45, 0.40, 0.35);
    vec3 darkRock     = vec3(0.25, 0.22, 0.18);
    vec3 snow         = vec3(0.92, 0.95, 1.00);

    if (surface.height01 < 0.30) {
        float blend = smoothstep(0.0, 0.30, surface.height01);
        surface.baseColor = mix(deepWater, shallowWater, blend);
    } else if (surface.height01 < 0.36) {
        float blend = smoothstep(0.30, 0.36, surface.height01);
        surface.baseColor = mix(shallowWater, sand, blend);
    } else if (surface.height01 < 0.55) {
        float blend = smoothstep(0.36, 0.55, surface.height01);
        surface.baseColor = mix(grass, darkGrass, blend);
    } else if (surface.height01 < 0.75) {
        float blend = smoothstep(0.55, 0.75, surface.height01);
        surface.baseColor = mix(darkGrass, rock, blend);
    } else if (surface.height01 < 0.88) {
        float blend = smoothstep(0.75, 0.88, surface.height01);
        surface.baseColor = mix(rock, darkRock, blend);
    } else {
        float blend = smoothstep(0.88, 1.0, surface.height01);
        surface.baseColor = mix(darkRock, snow, blend);
    }

    if (surface.slope > 0.45) {
        float rockBlend = smoothstep(0.45, 0.70, surface.slope);
        surface.baseColor = mix(surface.baseColor, darkRock, rockBlend);
    }

    return surface;
}
