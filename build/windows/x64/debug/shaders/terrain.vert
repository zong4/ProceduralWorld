#version 410 core

layout (location = 0) in vec2 aUV;

out vec2 vTexCoord;

void main()
{
    vTexCoord = aUV;
    gl_Position = vec4(aUV, 0.0, 1.0);
}