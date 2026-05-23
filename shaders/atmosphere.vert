#version 410 core

// 大气顶点阶段：绘制一个比星球半径更大的球壳。

layout (location = 0) in vec3 aPosition;

out vec3 vWorldPos;
out vec3 vSphereNormal;

uniform mat4 model;
uniform mat4 cameraRelativeView;
uniform mat4 projection;
uniform vec3 cameraPos;
uniform float atmosphereRadius;

void main()
{
    // 大气位置也使用 camera-relative view，保持与地形/海洋一致的精度策略。
    vec3 localPos = aPosition * atmosphereRadius;
    vec4 worldPos = model * vec4(localPos, 1.0);
    vWorldPos = worldPos.xyz;
    vSphereNormal = normalize(mat3(model) * aPosition);

    vec3 cameraRelativePos = worldPos.xyz - cameraPos;
    gl_Position = projection * cameraRelativeView * vec4(cameraRelativePos, 1.0);
}
