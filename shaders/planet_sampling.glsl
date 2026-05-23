// cube-sphere texture array 采样工具。
// CPU 将 6 个 cube face 上传为 sampler2DArray；这里负责球面方向和 face/uv 的互转，
// 并在 face 边界附近混合多个面，减少接缝。

struct FaceUv
{
    int face;
    vec2 uv;
};

int faceIndexFromDirection(vec3 dir)
{
    vec3 a = abs(dir);
    if (a.x >= a.y && a.x >= a.z) {
        return dir.x >= 0.0 ? 0 : 1;
    }
    if (a.y >= a.x && a.y >= a.z) {
        return dir.y >= 0.0 ? 2 : 3;
    }
    return dir.z >= 0.0 ? 4 : 5;
}

vec3 faceBasisNormal(int f)
{
    if (f == 0) return vec3( 1.0,  0.0,  0.0);
    if (f == 1) return vec3(-1.0,  0.0,  0.0);
    if (f == 2) return vec3( 0.0,  1.0,  0.0);
    if (f == 3) return vec3( 0.0, -1.0,  0.0);
    if (f == 4) return vec3( 0.0,  0.0,  1.0);
    return vec3(0.0, 0.0, -1.0);
}

vec3 faceBasisAxisU(int f)
{
    if (f == 0) return vec3( 0.0, 0.0, -1.0);
    if (f == 1) return vec3( 0.0, 0.0,  1.0);
    if (f == 2) return vec3( 1.0, 0.0,  0.0);
    if (f == 3) return vec3( 1.0, 0.0,  0.0);
    if (f == 4) return vec3( 1.0, 0.0,  0.0);
    return vec3(-1.0, 0.0, 0.0);
}

vec3 faceBasisAxisV(int f)
{
    if (f == 0) return vec3(0.0, 1.0,  0.0);
    if (f == 1) return vec3(0.0, 1.0,  0.0);
    if (f == 2) return vec3(0.0, 0.0, -1.0);
    if (f == 3) return vec3(0.0, 0.0,  1.0);
    if (f == 4) return vec3(0.0, 1.0,  0.0);
    return vec3(0.0, 1.0, 0.0);
}

FaceUv directionToFaceUv(vec3 dir)
{
    // 按最大绝对轴选择 cube face，再投影回该面的 UV。
    vec3 d = normalize(dir);
    int mappedFace = faceIndexFromDirection(d);
    vec3 n = faceBasisNormal(mappedFace);
    vec3 u = faceBasisAxisU(mappedFace);
    vec3 v = faceBasisAxisV(mappedFace);
    float projection = max(abs(dot(d, n)), 0.000001);
    vec3 cubePoint = d / projection;
    vec2 faceUv = vec2(dot(cubePoint - n, u), dot(cubePoint - n, v));
    FaceUv result;
    result.face = mappedFace;
    result.uv = clamp(faceUv * 0.5 + 0.5, vec2(0.0), vec2(1.0));
    return result;
}

vec2 faceUvForFace(int f, vec3 dir)
{
    vec3 d = normalize(dir);
    vec3 n = faceBasisNormal(f);
    vec3 u = faceBasisAxisU(f);
    vec3 v = faceBasisAxisV(f);
    float projection = max(abs(dot(d, n)), 0.000001);
    vec3 cubePoint = d / projection;
    vec2 faceUv = vec2(dot(cubePoint - n, u), dot(cubePoint - n, v));
    return clamp(faceUv * 0.5 + 0.5, vec2(0.0), vec2(1.0));
}

float cubeFaceBlendWidth()
{
    // 材质层使用较宽的跨面混合，隐藏颜色/权重接缝。
    return max(proceduralDataTexelSize * 2.5, 0.010);
}

float cubeFaceHeightBlendWidth()
{
    // 高度层使用较窄的混合，避免边界处几何被过度抹平。
    return max(proceduralDataTexelSize * 0.75, 0.0015);
}

float sampleFloatArrayOnFace(sampler2DArray tex, vec3 dir, int f)
{
    return texture(tex, vec3(faceUvForFace(f, dir), float(f))).r;
}

vec4 sampleVec4ArrayOnFace(sampler2DArray tex, vec3 dir, int f)
{
    return texture(tex, vec3(faceUvForFace(f, dir), float(f)));
}

float sampleFloatArraySeamlessWidth(sampler2DArray tex, vec3 dir, float width)
{
    // 在接近 cube 边/角时按三个主轴权重混合相邻 face 的采样值。
    vec3 d = normalize(dir);
    vec3 a = abs(d);
    float maxAxis = max(max(a.x, a.y), a.z);
    float wx = smoothstep(maxAxis - width, maxAxis, a.x);
    float wy = smoothstep(maxAxis - width, maxAxis, a.y);
    float wz = smoothstep(maxAxis - width, maxAxis, a.z);
    float sum = max(wx + wy + wz, 0.0001);

    float value = 0.0;
    value += sampleFloatArrayOnFace(tex, d, d.x >= 0.0 ? 0 : 1) * wx;
    value += sampleFloatArrayOnFace(tex, d, d.y >= 0.0 ? 2 : 3) * wy;
    value += sampleFloatArrayOnFace(tex, d, d.z >= 0.0 ? 4 : 5) * wz;
    return value / sum;
}

vec4 sampleVec4ArraySeamlessWidth(sampler2DArray tex, vec3 dir, float width)
{
    // vec4 版本用于 biome 权重和侵蚀 RGBA 数据。
    vec3 d = normalize(dir);
    vec3 a = abs(d);
    float maxAxis = max(max(a.x, a.y), a.z);
    float wx = smoothstep(maxAxis - width, maxAxis, a.x);
    float wy = smoothstep(maxAxis - width, maxAxis, a.y);
    float wz = smoothstep(maxAxis - width, maxAxis, a.z);
    float sum = max(wx + wy + wz, 0.0001);

    vec4 value = vec4(0.0);
    value += sampleVec4ArrayOnFace(tex, d, d.x >= 0.0 ? 0 : 1) * wx;
    value += sampleVec4ArrayOnFace(tex, d, d.y >= 0.0 ? 2 : 3) * wy;
    value += sampleVec4ArrayOnFace(tex, d, d.z >= 0.0 ? 4 : 5) * wz;
    return value / sum;
}

float sampleFloatArraySeamless(sampler2DArray tex, vec3 dir)
{
    return sampleFloatArraySeamlessWidth(tex, dir, cubeFaceBlendWidth());
}

float sampleFloatArraySeamlessNarrow(sampler2DArray tex, vec3 dir)
{
    return sampleFloatArraySeamlessWidth(tex, dir, cubeFaceHeightBlendWidth());
}

vec4 sampleVec4ArraySeamless(sampler2DArray tex, vec3 dir)
{
    return sampleVec4ArraySeamlessWidth(tex, dir, cubeFaceBlendWidth());
}
