#version 410 core

layout (location = 0) in vec2 aUV;

out vec2 vTexCoord;

uniform vec2 nodeUvMin;
uniform vec2 nodeUvSize;

void main()
{
    vTexCoord = nodeUvMin + aUV * nodeUvSize;
    gl_Position = vec4(aUV, 0.0, 1.0);
}
