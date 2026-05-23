#version 410 core

// 地形顶点阶段。
// 输入是一个复用的 patch 局部网格，CPU 每次 draw 只改变 nodeUvMin/nodeUvSize。
// 这里把局部 UV 映射到当前 quadtree node 的 face UV，并提前估算海平面球面位置。

layout (location = 0) in vec2 aUV;
layout (location = 1) in float aSkirt;

out vec2 vTexCoord;
out vec3 vWorldPos;
out float vSkirt;

uniform vec2 nodeUvMin;
uniform vec2 nodeUvSize;
uniform mat4 model;
uniform vec3 faceNormal;
uniform vec3 faceAxisU;
uniform vec3 faceAxisV;
uniform float seaLevelRadius;

void main()
{
    // 将 patch 局部 0..1 UV remap 到 cube face 的实际 node 范围。
    vTexCoord = nodeUvMin + aUV * nodeUvSize;
    vSkirt = aSkirt;
    // 顶点阶段的位置只用于 TCS 估算距离；最终位移发生在 TES。
    vec2 faceUV = vTexCoord * 2.0 - 1.0;
    vec3 cubePos = faceNormal + faceUV.x * faceAxisU + faceUV.y * faceAxisV;
    vec3 sphereDir = normalize(cubePos);
    vWorldPos = (model * vec4(sphereDir * seaLevelRadius, 1.0)).xyz;
    gl_Position = vec4(aUV, 0.0, 1.0);
}
