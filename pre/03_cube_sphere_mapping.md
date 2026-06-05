# 03. 六面体星球与接缝处理

## 对应图片
- ../images/03_cube_sphere_mapping.svg
- ../images/15_flow_03_cube_sphere_mapping.svg

## 讲解目标
用 cube face 组织数据，再映射到球面，降低球面采样复杂度。

## 关键代码位置
- src/PlanetProceduralData.cpp
- include/PlanetProceduralData.h

## 讲解流程
1. 把星球拆成 6 个 cube face
2. 每个 face 使用二维 texel 网格
3. face UV 转换为 cube 坐标
4. cubeSphereDirection() 归一化到球面方向
5. 用方向向量采样噪声和气候函数
6. 跨 face 查找 neighborCell()
7. fixCubeFaceSeams() 多轮边界融合
8. 输出连续的球面高度场

## 口播稿
> 直接在球面上做规则网格比较麻烦，所以项目采用 cube-sphere 思路：先把星球拆成六张方形贴图，每张贴图都是普通二维数组。

> 生成时，每个 texel 先得到它在 cube face 上的位置，再归一化成球面方向。之后所有噪声、温度、湿度和水文计算都基于这个球面方向。

> 难点在接缝。如果六个面分别生成，边界很容易出现高度断裂。

> 代码里用 neighborCell() 找到跨面的相邻 texel，再用 fixCubeFaceSeams() 对边界做多轮融合。

> 讲解时可以说：cube-sphere 是数据组织方式，接缝处理保证它最终看起来像一个连续星球。

## 老师可能追问时的回答
如果被问为什么不用 UV 球，可以回答：cube-sphere 的采样更均匀，适合 texture array 和 chunk 化处理。