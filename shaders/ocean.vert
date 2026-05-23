#version 410 core

// 海洋顶点阶段：与地形类似，只做 patch UV remap 和海平面球面位置预估。

layout (location = 0) in vec2 aUV;

out vec2 vTexCoord;
out vec4 vWorldPos;

uniform vec2 nodeUvMin;
uniform vec2 nodeUvSize;
uniform mat4 model;
uniform vec3 faceNormal;
uniform vec3 faceAxisU;
uniform vec3 faceAxisV;
uniform float seaLevelRadius;

void main()
{
    // 当前 quadtree patch 的局部 UV 转换为 cube face UV。
    vTexCoord = nodeUvMin + aUV * nodeUvSize;
    vec2 faceUV = vTexCoord * 2.0 - 1.0;
    vec3 cubePos = faceNormal + faceUV.x * faceAxisU + faceUV.y * faceAxisV;
    vec3 sphereDir = normalize(cubePos);
    vWorldPos = model * vec4(sphereDir * seaLevelRadius, 1.0);
    gl_Position = vec4(aUV, 0.0, 1.0);
}
