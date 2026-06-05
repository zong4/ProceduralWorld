# 02. 应用状态与生成线程

## 对应图片
- ../images/02_application_workflow.svg
- ../images/14_flow_02_app_state_generation_thread.svg

## 讲解目标
用三个阶段隔离参数设置、后台生成和实时渲染。

## 关键代码位置
- src/main.cpp

## 讲解流程
1. ProceduralSetup 显示参数设置界面
2. 点击 Generate Planet
3. startPlanetGeneration() 复制当前参数
4. std::async 在后台生成 PlanetProceduralData
5. progressCallback 更新原子进度值
6. 主循环继续刷新进度条
7. future ready 后调用 finishPlanetGeneration()
8. setProceduralData() 上传 GPU 并进入 Render

## 口播稿
> 这个模块体现了项目的工程组织方式。生成星球可能比较耗时，所以主线程不直接卡住等待，而是通过 std::async 放到后台执行。

> 后台线程只做 CPU 数据计算，比如高度场、水文和材质权重；它不碰 OpenGL 对象。

> 主循环通过 future 和 atomic 进度值更新 UI，因此生成时仍然可以显示进度条。

> 当 future 完成后，finishPlanetGeneration() 把结果交给 PlanetRenderer，主线程再创建纹理、VBO、IBO 和 VAO。

> 这样既避免 OpenGL 线程问题，也让应用状态从参数设置、生成中、渲染中变得很清楚。

## 老师可能追问时的回答
可以强调：线程拆分不是为了炫技，而是为了避免 UI 假死，并保证 OpenGL 资源只在主线程创建。