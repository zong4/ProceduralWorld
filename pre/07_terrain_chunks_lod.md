# 07. 地形 Chunk 烘焙与 Baked LOD

## 对应图片
- ../images/07_baked_chunk_lod.svg
- ../images/19_flow_07_terrain_chunks_lod.svg

## 讲解目标
把 DEM 预烘焙为多个块，运行时只绘制可见块。

## 关键代码位置
- src/PlanetProceduralData.cpp
- src/PlanetRenderer.cpp
- include/PlanetRenderer.h

## 讲解流程
1. buildTerrainChunks() 划分每个 face
2. 每个 chunk 生成顶点和索引
3. 记录 chunk 中心、半径和误差范围
4. buildVisibleBakedChunks() 按相机筛选
5. 视锥裁剪剔除不可见块
6. 根据距离选择 LOD
7. 绑定对应 VAO / IBO
8. drawBakedTerrainPass() 绘制

## 口播稿
> 如果把整个星球一次性画完，顶点数量会很大。项目把地形烘焙成 chunk，运行时只处理相机附近或视野内的块。

> 每个 chunk 有自己的顶点、索引、中心点和包围范围。

> 渲染前，buildVisibleBakedChunks() 根据相机、视锥和距离挑出当前需要绘制的块。

> LOD 的作用是近处保留更多细节，远处减少几何开销。

> 这部分可以强调为性能优化模块：视觉上仍然是完整星球，但 GPU 每帧只画必要部分。

## 老师可能追问时的回答
如果老师问为什么叫 baked，可以解释为：生成阶段已经把网格准备好，渲染阶段主要选择和提交。