# 17. 高度诊断工具

## 对应图片
- ../images/29_flow_17_height_diagnostics.svg

## 讲解目标
独立命令行工具用于检查高度范围和生成质量。

## 关键代码位置
- tools/TerrainHeightDiagnostics.cpp
- xmake.lua

## 讲解流程
1. 运行 TerrainHeightDiagnostics
2. 构造 PlanetProceduralSettings
3. 调用 PlanetProceduralData::generate()
4. 遍历 6 个 face 的 height 数据
5. 统计 min / max / average
6. 检查水体和陆地比例
7. 输出诊断结果
8. 辅助调整生成参数

## 口播稿
> 高度诊断工具是辅助开发用的，不需要打开完整渲染窗口。

> 它直接生成一份程序化数据，然后统计高度范围、平均值、水陆比例等信息。

> 这样可以快速发现生成参数是否异常，比如高度过平、海洋过多或陆地过少。

> 对答辩来说，这说明项目有验证工具，不完全依赖肉眼看画面。

> 如果后续继续扩展，也可以把更多指标加入这个工具，比如河道数量、侵蚀强度分布和材质覆盖率。

## 老师可能追问时的回答
这一页可以作为工程完整性的补充：有主程序，也有独立诊断工具。