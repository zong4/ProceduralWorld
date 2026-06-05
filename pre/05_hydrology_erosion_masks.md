# 05. 水文、侵蚀与河道 Mask

## 对应图片
- ../images/05_hydrology_erosion_masks.svg
- ../images/17_flow_05_hydrology_erosion_masks.svg

## 讲解目标
用下游汇流、stream power 和坡面扩散生成河道与侵蚀痕迹。

## 关键代码位置
- src/PlanetProceduralData.cpp

## 讲解流程
1. 根据高度对 texel 排序
2. 为每个 texel 寻找下游 receiver
3. 累加 drainage / flow accumulation
4. 根据 slope 和 drainage 计算 streamPower
5. 切割河道并写入 channel mask
6. 记录 wear / deposition / flow maps
7. thermal diffusion 平滑陡坡
8. 计算水深、岸线和湿度影响

## 口播稿
> 这个模块让地形不只是噪声堆出来的山，而是有水流塑造过的痕迹。

> 代码先按高度排序，让高处的水往低处汇流。每个格子会寻找下游 receiver，并累计 drainage。

> 当汇流量和坡度都足够高时，stream power 会变大，表示水流有能力切割河道。

> 切割结果会写入 channel、flow、wear 和 deposition 等调试层，后续可以影响材质显示和调试视图。

> 最后 thermal diffusion 会让过陡坡面变得更自然，水深和岸线数据则服务于海岸、湿度和海洋混合。

## 老师可能追问时的回答
可以把这个模块讲成：先决定水往哪里走，再决定水对地形切多少。