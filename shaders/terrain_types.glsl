// 地形片元阶段共享的数据结构。

struct SurfaceData
{
    // 经过 biome/坡度/侵蚀混合后的基础反照率。
    vec3 baseColor;
    float riverMask;
    float riverSpecular;
    // 由法线与径向方向估算的坡度，0=平缓，1=垂直峭壁。
    float slope;
    float height01;
    float radialAlignment;
};

struct LightingData
{
    // tone mapping 前的光照结果。
    vec3 litColor;
    float fogFactor;
};
