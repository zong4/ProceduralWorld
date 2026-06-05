# 12. 大气散射 LUT 与天空

## 对应图片
- ../images/24_flow_12_atmosphere_lut.svg

## 讲解目标
预计算大气散射查找表，运行时快速渲染天空和远景。

## 关键代码位置
- src/PlanetRenderer.cpp
- shaders/atmosphere*.frag
- shaders/atmosphere*.vert

## 讲解流程
1. 根据大气参数计算 LUT signature
2. 参数变化时触发 precomputeAtmosphereLuts()
3. 计算 transmittance LUT
4. 计算 irradiance LUT
5. 计算 scattering LUT
6. 运行时 atmosphere.frag 采样 LUT
7. 根据太阳方向、视线和高度计算天空颜色
8. 与地形、海洋和云层合成

## 口播稿
> 大气散射如果每个像素都完整积分会很贵，所以项目采用 LUT 预计算。

> 当大气参数变化时，渲染器生成一个 signature，判断 LUT 是否需要重算。

> 预计算阶段会得到透射率、辐照度和散射相关纹理。

> 运行时 shader 通过查表快速得到天空颜色、地平线雾化和远景衰减。

> 这个模块的价值是把复杂物理近似前置，保证实时帧率。

## 老师可能追问时的回答
讲解时可以把 LUT 说成提前算好的表：用一点预处理换每帧渲染速度。