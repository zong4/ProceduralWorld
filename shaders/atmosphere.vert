#version 410 core

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
    vec3 localPos = aPosition * atmosphereRadius;
    vec4 worldPos = model * vec4(localPos, 1.0);
    vWorldPos = worldPos.xyz;
    vSphereNormal = normalize(mat3(model) * aPosition);

    vec3 cameraRelativePos = worldPos.xyz - cameraPos;
    gl_Position = projection * cameraRelativeView * vec4(cameraRelativePos, 1.0);
}
