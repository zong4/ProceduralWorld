# 08. CPU 数据到 GPU 的上传

## 对应图片
- ../images/06_gpu_data_upload.svg
- ../images/20_flow_08_gpu_upload.svg

## 讲解目标
把生成好的多层数据转成 texture array 和 GPU mesh。

## 关键代码位置
- src/PlanetRenderer.cpp
- include/PlanetRenderer.h

## 讲解流程
1. setProceduralData() 接收 CPU 数据
2. 检查 face count 和 resolution
3. 创建 height texture array
4. 创建 water / climate / mask texture array
5. 创建材质权重 texture array
6. 上传 baked terrain vertex/index buffers
7. 初始化或刷新海洋、大气相关资源
8. 渲染器标记数据 ready

## 口播稿
> 这个模块是 CPU 生成和 GPU 渲染之间的桥。

> CPU 数据本质上是多个数组，而 shader 更适合通过纹理采样。因此代码把六个 cube face 打包成 texture array。

> 高度、水体、温度、湿度、侵蚀、河道和材质权重可以作为不同纹理或不同通道上传。

> 地形 chunk 的顶点和索引则上传到 VBO、IBO、VAO。

> 上传完成后，渲染器每帧就不再重新计算地形，只需要采样纹理和绘制 mesh。

## 老师可能追问时的回答
重点解释 texture array：六个面作为 layer，shader 根据 face/layer 取对应数据。