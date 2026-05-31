#version 410 core

layout (location = 0) in vec3 aLocalPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec3 aSphereDir;
layout (location = 3) in float aHeight;

out vec3 teWorldPos;
out vec3 teNormal;
out vec3 teSphereDir;
out float teHeight;
out float teSurfaceHeight;
out float teSkirt;

uniform mat4 model;
uniform mat4 cameraRelativeView;
uniform mat4 projection;
uniform vec3 cameraPos;
uniform vec4 clipPlane;

void main()
{
    vec4 worldPos = model * vec4(aLocalPos, 1.0);
    teWorldPos = worldPos.xyz;
    teNormal = normalize(mat3(transpose(inverse(model))) * aNormal);
    teSphereDir = normalize(aSphereDir);
    teHeight = aHeight;
    teSurfaceHeight = aHeight;
    teSkirt = 0.0;

    vec4 relativeWorldPos = vec4(worldPos.xyz - cameraPos, 1.0);
    gl_Position = projection * cameraRelativeView * relativeWorldPos;
    gl_ClipDistance[0] = dot(worldPos, clipPlane);
}
