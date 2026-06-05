# 11. 海洋材质、反射、折射与水深混合

## 对应图片
- ../images/09_ocean_reflection_refraction.svg
- ../images/23_flow_11_ocean_material.svg

## 讲解目标
用反射/折射 FBO、Fresnel 和深浅水颜色混合增强真实感。

## 关键代码位置
- src/PlanetRenderer.cpp
- shaders/ocean.frag

## 讲解流程
1. drawReflectionRefractionPasses() 创建离屏画面
2. 反射 pass 渲染天空和地形反射
3. 折射 pass 渲染水下/透过水面颜色
4. 主海洋 pass 采样 FFT 波浪纹理
5. 根据视角计算 Fresnel
6. 根据水深混合浅水和深水颜色
7. 加入高光、泡沫、SSS 和大气远景
8. 输出最终水面颜色

## 口播稿
> 这个模块解释为什么海水看起来不只是蓝色平面。

> 渲染器先做反射和折射两个离屏 pass，得到水面应该反射什么、透过水面能看到什么。

> 主海洋 pass 再结合 FFT 波浪法线，计算 Fresnel。视角越贴近水面，反射越强；俯视时折射和水体颜色更明显。

> 水深也会影响颜色，浅水更亮更偏透明，深水更暗更饱和。

> 最后再叠加高光、泡沫、次表面散射和大气远景，让海洋融入整个星球画面。

## 老师可能追问时的回答
可以强调它是多 pass 渲染：不是一次 shader 直接凭空画海，而是先准备反射/折射纹理。