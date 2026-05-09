#version 410 core

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
    vTexCoord = nodeUvMin + aUV * nodeUvSize;
    vSkirt = aSkirt;
    vec2 faceUV = vTexCoord * 2.0 - 1.0;
    vec3 cubePos = faceNormal + faceUV.x * faceAxisU + faceUV.y * faceAxisV;
    vec3 sphereDir = normalize(cubePos);
    vWorldPos = (model * vec4(sphereDir * seaLevelRadius, 1.0)).xyz;
    gl_Position = vec4(aUV, 0.0, 1.0);
}
