# 10. FFT 海浪

## 对应图片
- ../images/08_ocean_fft.svg
- ../images/22_flow_10_fft_ocean.svg

## 讲解目标
用频域海浪谱生成高度、法线、位移和 folding 纹理。

## 关键代码位置
- src/FFTOcean.cpp
- include/FFTOcean.h

## 讲解流程
1. initialize() 配置 cascades 和分辨率
2. buildInitialSpectrum() 生成 Phillips spectrum
3. 根据 wind、amplitude、gravity 初始化频域
4. update(time) 推进频域相位
5. IFFT 转回空间域
6. 计算 height / normal / displacement / folding
7. uploadTextures() 上传 GPU
8. ocean shader 采样形成动态波浪

## 口播稿
> FFT 海洋模块负责让水面真正动起来。

> 它先在频域生成初始海浪谱，参数包括风向、风速、振幅和重力等。

> 每一帧根据时间更新频域相位，再通过 IFFT 得到空间域波形。

> 输出不只有高度，还有法线、水平位移和 folding 信息。

> 这些纹理上传到 GPU 后，海洋 shader 就可以在不同距离采样不同 cascade，形成从近景波浪到远景起伏的连续效果。

## 老师可能追问时的回答
讲 FFT 时不用展开数学细节，重点说：频域生成复杂波形，IFFT 转成水面纹理。