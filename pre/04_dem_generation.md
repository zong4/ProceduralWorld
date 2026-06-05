# 04. DEM 程序化地形生成

## 对应图片
- ../images/04_dem_generation.svg
- ../images/16_flow_04_dem_generation.svg

## 讲解目标
当前实际运行路径是 generateDemPrototype()，用于生成高度、水文和地表属性。

## 关键代码位置
- src/PlanetProceduralData.cpp

## 讲解流程
1. PlanetProceduralData::generate()
2. clamp faceResolution
3. 进入 generateDemPrototype()
4. 为 6 个 face 分配高度、水深、侵蚀等数组
5. 遍历每个 texel 并计算 sphereDir
6. fBM / ridgedFbm 生成大陆和山脉
7. 记录 height、uplift、landMask、preErosionHeight
8. 接缝融合、水文侵蚀、气候计算
9. buildTerrainChunks() 烘焙地形块

## 口播稿
> 这一页要特别讲清楚当前真实代码路径。PlanetProceduralData::generate() 进入 generateDemPrototype() 后直接 return，因此现在运行的是 DEM 原型路径。

> DEM 可以理解为数字高程模型。代码给六个 cube face 分配多张数组，包括高度、水深、侵蚀、温度、湿度、河道和材质权重。

> 每个 texel 通过球面方向采样多层噪声。低频噪声决定大陆轮廓，中频和高频噪声塑造山脉、山脊和局部起伏。

> 得到基础地形后，代码再进行接缝修正、水文侵蚀和气候字段计算。

> 最后 buildTerrainChunks() 把 DEM 数据烘焙成可渲染的地形块，这一步把生成阶段和渲染阶段连接起来。

## 老师可能追问时的回答
不要把后面保留的旧版分模块生成代码说成当前主路径；它现在是保留代码，当前入口已经提前返回。