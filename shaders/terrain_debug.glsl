// renderMode 调试输出：
// 1=无光照材质色，2=高度灰度，3=法线可视化，其余返回 -1 表示正常光照。
vec4 debugSurfaceOutput(int renderMode, SurfaceData surface, vec3 shadingNormal)
{
    if (renderMode == 1) {
        return vec4(surface.baseColor, 1.0);
    }

    if (renderMode == 3) {
        return vec4(normalize(shadingNormal) * 0.5 + 0.5, 1.0);
    }

    if (renderMode == 4) {
        return vec4(surface.materialDebugColor, 1.0);
    }

    if (renderMode == 2) {
        return vec4(vec3(surface.height01), 1.0);
    }

    return vec4(-1.0);
}
