# 13. 程序化体积云

## 对应图片
- ../images/10_atmosphere_clouds.svg
- ../images/25_flow_13_procedural_clouds.svg

## 讲解目标
在大气 shader 中用噪声和 raymarch 生成可调体积云。

## 关键代码位置
- shaders/atmosphere.frag
- src/main.cpp

## 讲解流程
1. UI 控制 clouds 开关和参数
2. 传入 coverage、density、height、thickness
3. 在 atmosphere.frag 中构造云层区域
4. 沿视线进行 raymarch
5. 多层噪声计算云密度
6. light march 估算光照和阴影
7. 与天空散射颜色混合
8. 输出带体积感的云层

## 口播稿
> 云层属于视觉增强模块，但它和大气放在一起很合理，因为云需要天空颜色和太阳方向。

> UI 提供覆盖率、锐度、缩放、高度、厚度、密度、步数等参数。

> shader 在大气层中定义一段云层高度范围，然后沿相机视线采样多个点。

> 每个点通过噪声得到云密度，再沿太阳方向做简化 light march，估算云内部受光程度。

> 最后把云颜色和原本天空散射结果混合，得到可调的程序化体积云。

## 老师可能追问时的回答
可以补一句：步数越高越细腻，但开销也越大，所以 UI 里保留了质量参数。