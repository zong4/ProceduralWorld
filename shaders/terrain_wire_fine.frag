#version 410 core

// 地形细线框叠加。跳过 skirt 和水下地形，只显示陆地 tessellation 网格。

in float teSurfaceHeight;
in float teSkirt;

out vec4 FragColor;

uniform float seaLevelOffset;

void main()
{
    if (teSkirt > 0.15 || teSurfaceHeight <= seaLevelOffset + 0.0005) {
        discard;
    }

    FragColor = vec4(1.0, 0.55, 0.10, 0.45);
}
