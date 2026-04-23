#version 410 core

layout(quads, fractional_even_spacing, ccw) in;

in vec2 tcTexCoord[];

out vec3 teWorldPos;
out vec3 teNormal;
out vec2 teTexCoord;
out float teHeight;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform vec3 faceNormal;
uniform vec3 faceAxisU;
uniform vec3 faceAxisV;
uniform float time;
uniform float planetRadius;
uniform float heightScale;
uniform float noiseScale;

vec3 hash3(vec3 p)
{
    p = vec3(
        dot(p, vec3(127.1, 311.7,  74.7)),
        dot(p, vec3(269.5, 183.3, 246.1)),
        dot(p, vec3(113.5, 271.9, 124.6))
    );
    return -1.0 + 2.0 * fract(sin(p) * 43758.5453123);
}

float fade(float t) { return t * t * t * (t * (t * 6.0 - 15.0) + 10.0); }
vec3  fade(vec3 t)  { return t * t * t * (t * (t * 6.0 - 15.0) + 10.0); }

float gradientNoise(vec3 p)
{
    vec3 i = floor(p);
    vec3 f = fract(p);
    vec3 u = fade(f);

    float n000 = dot(hash3(i + vec3(0.0, 0.0, 0.0)), f - vec3(0.0, 0.0, 0.0));
    float n100 = dot(hash3(i + vec3(1.0, 0.0, 0.0)), f - vec3(1.0, 0.0, 0.0));
    float n010 = dot(hash3(i + vec3(0.0, 1.0, 0.0)), f - vec3(0.0, 1.0, 0.0));
    float n110 = dot(hash3(i + vec3(1.0, 1.0, 0.0)), f - vec3(1.0, 1.0, 0.0));
    float n001 = dot(hash3(i + vec3(0.0, 0.0, 1.0)), f - vec3(0.0, 0.0, 1.0));
    float n101 = dot(hash3(i + vec3(1.0, 0.0, 1.0)), f - vec3(1.0, 0.0, 1.0));
    float n011 = dot(hash3(i + vec3(0.0, 1.0, 1.0)), f - vec3(0.0, 1.0, 1.0));
    float n111 = dot(hash3(i + vec3(1.0, 1.0, 1.0)), f - vec3(1.0, 1.0, 1.0));

    float nx00 = mix(n000, n100, u.x);
    float nx10 = mix(n010, n110, u.x);
    float nx01 = mix(n001, n101, u.x);
    float nx11 = mix(n011, n111, u.x);
    float nxy0 = mix(nx00, nx10, u.y);
    float nxy1 = mix(nx01, nx11, u.y);
    return mix(nxy0, nxy1, u.z);
}

float fbm(vec3 p, int octaves, float lacunarity, float gain)
{
    float value = 0.0;
    float amplitude = 0.5;
    float frequency = 1.0;
    float maxValue = 0.0;

    for (int i = 0; i < octaves; ++i)
    {
        value += amplitude * gradientNoise(p * frequency);
        maxValue += amplitude;
        amplitude *= gain;
        frequency *= lacunarity;
    }

    return value / max(maxValue, 1e-5);
}

vec3 cubeFacePoint(vec2 uv)
{
    vec2 faceUV = uv * 2.0 - 1.0;
    return faceNormal + faceUV.x * faceAxisU + faceUV.y * faceAxisV;
}

float terrainHeight(vec3 sphereDir)
{
    vec3 p = sphereDir * noiseScale;

    vec3 warp = vec3(
        fbm(p + vec3(3.1, 0.0, 0.0), 4, 2.0, 0.5),
        fbm(p + vec3(0.0, 4.7, 0.0), 4, 2.0, 0.5),
        fbm(p + vec3(0.0, 0.0, 5.3), 4, 2.0, 0.5)
    );

    float continents = fbm(p + 1.8 * warp, 5, 2.0, 0.5);
    float ridges = 1.0 - abs(gradientNoise(p * 1.7 + warp * 2.0));
    ridges = pow(ridges, 3.0);
    float detail = fbm(p * 4.0 + warp * 3.0, 4, 2.2, 0.45);

    float h = continents * 0.9 + ridges * 0.55 + detail * 0.25;
    h = sign(h) * pow(abs(h), 1.15);
    return h;
}

vec3 displacedPosition(vec3 sphereDir)
{
    float h = terrainHeight(sphereDir);
    return sphereDir * (planetRadius + h * heightScale);
}

vec3 computeNormal(vec3 sphereDir)
{
    vec3 tangent = normalize(
        abs(sphereDir.y) < 0.99
            ? cross(vec3(0.0, 1.0, 0.0), sphereDir)
            : cross(vec3(1.0, 0.0, 0.0), sphereDir)
    );
    vec3 bitangent = normalize(cross(sphereDir, tangent));
    float eps = 0.0035;

    vec3 pL = displacedPosition(normalize(sphereDir - tangent * eps));
    vec3 pR = displacedPosition(normalize(sphereDir + tangent * eps));
    vec3 pD = displacedPosition(normalize(sphereDir - bitangent * eps));
    vec3 pU = displacedPosition(normalize(sphereDir + bitangent * eps));

    return normalize(cross(pR - pL, pU - pD));
}

void main()
{
    vec2 uv0 = mix(tcTexCoord[0], tcTexCoord[1], gl_TessCoord.x);
    vec2 uv1 = mix(tcTexCoord[3], tcTexCoord[2], gl_TessCoord.x);
    vec2 uv = mix(uv0, uv1, gl_TessCoord.y);

    teTexCoord = uv;

    vec3 cubePos = cubeFacePoint(uv);
    vec3 sphereDir = normalize(cubePos);
    float h = terrainHeight(sphereDir);
    teHeight = h;

    vec3 localPos = sphereDir * (planetRadius + h * heightScale);
    vec4 worldPos = model * vec4(localPos, 1.0);
    teWorldPos = worldPos.xyz;

    vec3 localNormal = computeNormal(sphereDir);
    teNormal = normalize(mat3(transpose(inverse(model))) * localNormal);

    gl_Position = projection * view * worldPos;
}
