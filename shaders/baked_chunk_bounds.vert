#version 410 core

layout (location = 0) in vec3 aLocalPos;

uniform mat4 model;
uniform mat4 cameraRelativeView;
uniform mat4 projection;
uniform vec3 cameraPos;

void main()
{
    vec4 worldPos = model * vec4(aLocalPos, 1.0);
    vec4 relativeWorldPos = vec4(worldPos.xyz - cameraPos, 1.0);
    gl_Position = projection * cameraRelativeView * relativeWorldPos;
}
