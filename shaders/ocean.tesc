#version 410 core

layout(vertices = 4) out;

in vec2 vTexCoord[];
out vec2 tcTexCoord[];

uniform vec3 cameraPos;
uniform mat4 model;
uniform vec3 faceNormal;
uniform vec3 faceAxisU;
uniform vec3 faceAxisV;
uniform float seaLevelRadius;
uniform float tessMin;
uniform float tessMax;
uniform float tessMinDist;
uniform float tessMaxDist;

vec3 oceanSpherePosition(vec2 uv)
{
    vec2 faceUV = uv * 2.0 - 1.0;
    vec3 cubePos = faceNormal + faceUV.x * faceAxisU + faceUV.y * faceAxisV;
    vec3 sphereDir = normalize(cubePos);
    return sphereDir * seaLevelRadius;
}

float computeTessLevel(vec2 uv)
{
    vec4 worldPos = model * vec4(oceanSpherePosition(uv), 1.0);
    float dist = length(cameraPos - worldPos.xyz);
    float t = clamp((dist - tessMinDist) / (tessMaxDist - tessMinDist), 0.0, 1.0);
    return mix(tessMax, tessMin, t);
}

void main()
{
    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
    tcTexCoord[gl_InvocationID] = vTexCoord[gl_InvocationID];

    if (gl_InvocationID == 0)
    {
        vec2 mid0 = (vTexCoord[0] + vTexCoord[1]) * 0.5;
        vec2 mid1 = (vTexCoord[1] + vTexCoord[2]) * 0.5;
        vec2 mid2 = (vTexCoord[2] + vTexCoord[3]) * 0.5;
        vec2 mid3 = (vTexCoord[3] + vTexCoord[0]) * 0.5;
        vec2 center = (vTexCoord[0] + vTexCoord[1] + vTexCoord[2] + vTexCoord[3]) * 0.25;

        gl_TessLevelOuter[0] = computeTessLevel(mid3);
        gl_TessLevelOuter[1] = computeTessLevel(mid0);
        gl_TessLevelOuter[2] = computeTessLevel(mid1);
        gl_TessLevelOuter[3] = computeTessLevel(mid2);
        gl_TessLevelInner[0] = computeTessLevel(center);
        gl_TessLevelInner[1] = computeTessLevel(center);
    }
}
