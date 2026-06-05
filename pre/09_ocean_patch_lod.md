# 09. 海洋 Patch LOD 与球面海面

## 对应图片
- ../images/21_flow_09_ocean_patch_lod.svg

## 讲解目标
在球面上动态选择海洋 patch，并用 tessellation 提升近处细节。

## 关键代码位置
- src/PlanetRenderer.cpp
- shaders/ocean.tesc
- shaders/ocean.tese
- shaders/ocean.frag

## 讲解流程
1. 从 6 个 cube face 创建 ocean root patch
2. collectVisibleOceanPatches() 遍历四叉树
3. 视锥裁剪排除不可见 patch
4. analyzePatchWaterCoverage() 判断水域比例
5. shouldSplitNode() 根据距离和误差细分
6. 生成可见 ocean patch 列表
7. tessellation shader 球面细分
8. fragment shader 计算水面颜色

## 口播稿
> 海洋不是一张固定平面，而是围绕星球的球面 patch。

> 每个 cube face 有海洋根节点，运行时通过四叉树选择可见区域。

> 如果 patch 不在视野里，或者水域覆盖率太低，就可以跳过。

> 近处 patch 会继续细分，远处 patch 保持粗略，从而节省绘制成本。

> tessellation shader 负责把 patch 细分并贴合球面，fragment shader 再根据 FFT 波浪、反射、折射和水深计算颜色。

## 老师可能追问时的回答
可以把海洋 LOD 和地形 chunk LOD 对比：地形是 baked chunk，海洋是运行时 quadtree patch。