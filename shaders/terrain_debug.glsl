vec4 debugSurfaceOutput(int renderMode, SurfaceData surface, vec3 shadingNormal)
{
    if (renderMode == 3) {
        return vec4(normalize(shadingNormal) * 0.5 + 0.5, 1.0);
    }

    if (renderMode == 2) {
        return vec4(vec3(surface.height01), 1.0);
    }

    return vec4(-1.0);
}
