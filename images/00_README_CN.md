# images 目录说明（中文）

这里按答辩使用顺序放置示意图和流程图。01-12 是概念示意图，13-31 是功能模块流程图。

## 概念示意图
- 01_overall_pipeline.svg：项目总架构
- 02_application_workflow.svg：应用状态与生成线程
- 03_cube_sphere_mapping.svg：六面体星球映射
- 04_dem_generation.svg：DEM 地形生成
- 05_hydrology_erosion_masks.svg：水文侵蚀示意
- 06_gpu_data_upload.svg：CPU 到 GPU 上传
- 07_baked_chunk_lod.svg：地形块 LOD
- 08_ocean_fft.svg：FFT 海洋
- 09_ocean_reflection_refraction.svg：海洋反射折射
- 10_atmosphere_clouds.svg：大气与体积云
- 11_frame_render_sequence.svg：每帧渲染顺序
- 12_debug_and_presentation.svg：调试与讲解支持

## 功能模块流程图
- 13_flow_01_project_overview.svg：项目总体流程
- 14_flow_02_app_state_generation_thread.svg：应用状态与生成线程
- 15_flow_03_cube_sphere_mapping.svg：六面体星球与接缝处理
- 16_flow_04_dem_generation.svg：DEM 程序化地形生成
- 17_flow_05_hydrology_erosion_masks.svg：水文、侵蚀与河道 Mask
- 18_flow_06_climate_materials.svg：温湿度、材质与地表分类
- 19_flow_07_terrain_chunks_lod.svg：地形 Chunk 烘焙与 Baked LOD
- 20_flow_08_gpu_upload.svg：CPU 数据到 GPU 的上传
- 21_flow_09_ocean_patch_lod.svg：海洋 Patch LOD 与球面海面
- 22_flow_10_fft_ocean.svg：FFT 海浪
- 23_flow_11_ocean_material.svg：海洋材质、反射、折射与水深混合
- 24_flow_12_atmosphere_lut.svg：大气散射 LUT 与天空
- 25_flow_13_procedural_clouds.svg：程序化体积云
- 26_flow_14_debug_visualization.svg：调试、可视化与性能面板
- 27_flow_15_input_camera.svg：输入与相机控制
- 28_flow_16_shader_resources.svg：Shader 编译与资源管理
- 29_flow_17_height_diagnostics.svg：高度诊断工具
- 30_flow_18_frame_render_sequence.svg：每帧渲染顺序
- 31_flow_19_presentation_summary.svg：答辩总结与边界说明