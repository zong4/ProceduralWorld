#version 410 core

// Input patch: 4 control points (quad patch)
layout(vertices = 4) out;

in vec2 vTexCoord[];
out vec2 tcTexCoord[];

uniform vec3 cameraPos;
uniform mat4 model;
uniform vec3 faceNormal;
uniform vec3 faceAxisU;
uniform vec3 faceAxisV;
uniform float planetRadius;
uniform float tessMin;       // Minimum tessellation level (far)
uniform float tessMax;       // Maximum tessellation level (near)
uniform float tessMinDist;   // Distance at which max tess starts
uniform float tessMaxDist;   // Distance beyond which min tess is used

vec3 cubeSpherePosition(vec2 uv)
{
    vec2 faceUV = uv * 2.0 - 1.0;
    vec3 cubePos = faceNormal + faceUV.x * faceAxisU + faceUV.y * faceAxisV;
    vec3 sphereDir = normalize(cubePos);
    return sphereDir * planetRadius;
}

// Compute tessellation level based on distance from camera
float computeTessLevel(vec2 uv)
{
    vec4 worldPos = model * vec4(cubeSpherePosition(uv), 1.0);
    float dist = length(cameraPos - worldPos.xyz);
    
    // Linearly interpolate based on distance
    float t = clamp((dist - tessMinDist) / (tessMaxDist - tessMinDist), 0.0, 1.0);
    return mix(tessMax, tessMin, t);
}

void main()
{
    // Pass-through control point data
    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
    tcTexCoord[gl_InvocationID] = vTexCoord[gl_InvocationID];

    // Only compute tessellation levels once (invocation 0)
    if (gl_InvocationID == 0)
    {
        // Compute levels at edge midpoints for smooth transitions
        // Edge 0: bottom (v=0), vertices 0 and 1
        vec2 mid0 = (vTexCoord[0] + vTexCoord[1]) * 0.5;
        // Edge 1: right  (u=1), vertices 1 and 2
        vec2 mid1 = (vTexCoord[1] + vTexCoord[2]) * 0.5;
        // Edge 2: top    (v=1), vertices 2 and 3
        vec2 mid2 = (vTexCoord[2] + vTexCoord[3]) * 0.5;
        // Edge 3: left   (u=0), vertices 3 and 0
        vec2 mid3 = (vTexCoord[3] + vTexCoord[0]) * 0.5;
        // Interior: patch center
        vec2 center = (vTexCoord[0] + vTexCoord[1] + vTexCoord[2] + vTexCoord[3]) * 0.25;

        gl_TessLevelOuter[0] = computeTessLevel(mid3);
        gl_TessLevelOuter[1] = computeTessLevel(mid0);
        gl_TessLevelOuter[2] = computeTessLevel(mid1);
        gl_TessLevelOuter[3] = computeTessLevel(mid2);

        gl_TessLevelInner[0] = computeTessLevel(center);
        gl_TessLevelInner[1] = computeTessLevel(center);

    }
}
