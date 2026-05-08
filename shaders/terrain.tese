#version 410 core

layout(quads, fractional_even_spacing, ccw) in;

in vec2 tcTexCoord[];

out vec3 teWorldPos;
out vec3 teNormal;
out vec2 teTexCoord;
out float teHeight;
out vec4 teClipSpacePos;

uniform mat4 model;
uniform mat4 cameraRelativeView;
uniform mat4 projection;
uniform vec3 cameraPos;
uniform vec3 faceNormal;
uniform vec3 faceAxisU;
uniform vec3 faceAxisV;
uniform float planetRadius;
uniform float heightScale;
uniform vec4 clipPlane;
uniform int faceIndex;
uniform float proceduralDataTexelSize;
uniform sampler2DArray proceduralHeightTexture;

vec3 cubeFacePoint(vec2 uv)
{
    vec2 faceUV = uv * 2.0 - 1.0;
    return faceNormal + faceUV.x * faceAxisU + faceUV.y * faceAxisV;
}

float terrainHeightAtUv(vec2 uv)
{
    vec2 clampedUv = clamp(uv, vec2(0.0), vec2(1.0));
    return texture(proceduralHeightTexture, vec3(clampedUv, float(faceIndex))).r;
}

vec3 displacedPositionFromUv(vec2 uv)
{
    vec3 sphereDir = normalize(cubeFacePoint(uv));
    float h = terrainHeightAtUv(uv);
    return sphereDir * (planetRadius + h * heightScale);
}

vec3 computeNormal(vec2 uv)
{
    float eps = max(proceduralDataTexelSize, 0.0035);
    vec2 uvL = clamp(uv - vec2(eps, 0.0), vec2(0.0), vec2(1.0));
    vec2 uvR = clamp(uv + vec2(eps, 0.0), vec2(0.0), vec2(1.0));
    vec2 uvD = clamp(uv - vec2(0.0, eps), vec2(0.0), vec2(1.0));
    vec2 uvU = clamp(uv + vec2(0.0, eps), vec2(0.0), vec2(1.0));

    vec3 pL = displacedPositionFromUv(uvL);
    vec3 pR = displacedPositionFromUv(uvR);
    vec3 pD = displacedPositionFromUv(uvD);
    vec3 pU = displacedPositionFromUv(uvU);

    return normalize(cross(pR - pL, pU - pD));
}

void main()
{
    vec2 uv0 = mix(tcTexCoord[0], tcTexCoord[1], gl_TessCoord.x);
    vec2 uv1 = mix(tcTexCoord[3], tcTexCoord[2], gl_TessCoord.x);
    vec2 uv = mix(uv0, uv1, gl_TessCoord.y);

    teTexCoord = uv;

    vec3 sphereDir = normalize(cubeFacePoint(uv));
    float h = terrainHeightAtUv(uv);
    teHeight = h;

    vec3 localPos = sphereDir * (planetRadius + h * heightScale);
    vec4 worldPos = model * vec4(localPos, 1.0);
    teWorldPos = worldPos.xyz;

    vec3 localNormal = computeNormal(uv);
    teNormal = normalize(mat3(transpose(inverse(model))) * localNormal);

    vec4 relativeWorldPos = vec4(worldPos.xyz - cameraPos, 1.0);
    teClipSpacePos = projection * cameraRelativeView * relativeWorldPos;
    gl_Position = teClipSpacePos;
    gl_ClipDistance[0] = dot(worldPos, clipPlane);
}
