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
uniform float time;
uniform float heightScale;
uniform float noiseScale;

// -------------------------------------------------------
// Noise utilities
// -------------------------------------------------------

// Hash function
vec2 hash2(vec2 p)
{
    p = vec2(dot(p, vec2(127.1, 311.7)), dot(p, vec2(269.5, 183.3)));
    return -1.0 + 2.0 * fract(sin(p) * 43758.5453123);
}

float hash(vec2 p)
{
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

// Smooth interpolation (quintic)
float fade(float t) { return t * t * t * (t * (t * 6.0 - 15.0) + 10.0); }
vec2  fade(vec2 t)  { return t * t * t * (t * (t * 6.0 - 15.0) + 10.0); }

// Gradient noise (Perlin-style)
float gradientNoise(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = fade(f);

    float a = dot(hash2(i + vec2(0.0, 0.0)), f - vec2(0.0, 0.0));
    float b = dot(hash2(i + vec2(1.0, 0.0)), f - vec2(1.0, 0.0));
    float c = dot(hash2(i + vec2(0.0, 1.0)), f - vec2(0.0, 1.0));
    float d = dot(hash2(i + vec2(1.0, 1.0)), f - vec2(1.0, 1.0));

    return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

// Value noise
float valueNoise(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = fade(f);

    float a = hash(i + vec2(0.0, 0.0));
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));

    return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

// Fractal Brownian Motion (fBm) — layered octaves of noise
float fbm(vec2 p, int octaves, float lacunarity, float gain)
{
    float value = 0.0;
    float amplitude = 0.5;
    float frequency = 1.0;
    float maxValue = 0.0;

    for (int i = 0; i < octaves; i++)
    {
        value     += amplitude * gradientNoise(p * frequency);
        maxValue  += amplitude;
        amplitude *= gain;
        frequency *= lacunarity;
    }
    return value / maxValue;
}

// Domain-warped fBm for more realistic terrain
float terrainHeight(vec2 uv)
{
    vec2 p = uv * noiseScale;

    // Domain warping: warp input coordinates with another fBm
    vec2 q = vec2(
        fbm(p + vec2(0.0,  0.0), 5, 2.0, 0.5),
        fbm(p + vec2(5.2,  1.3), 5, 2.0, 0.5)
    );

    vec2 r = vec2(
        fbm(p + 4.0 * q + vec2(1.7, 9.2), 5, 2.0, 0.5),
        fbm(p + 4.0 * q + vec2(8.3, 2.8), 5, 2.0, 0.5)
    );

    float h = fbm(p + 4.0 * r, 6, 2.0, 0.45);

    // Add ridge noise for mountain peaks
    float ridge = 1.0 - abs(gradientNoise(p * 0.5 + vec2(3.1, 4.7)));
    ridge = pow(ridge, 2.5);

    h = mix(h, ridge, 0.3);

    // Flatten valleys slightly (power curve)
    h = sign(h) * pow(abs(h), 0.8);

    return h;
}

// Compute normal via finite differences
vec3 computeNormal(vec2 uv, float eps)
{
    float hL = terrainHeight(uv - vec2(eps, 0.0));
    float hR = terrainHeight(uv + vec2(eps, 0.0));
    float hD = terrainHeight(uv - vec2(0.0, eps));
    float hU = terrainHeight(uv + vec2(0.0, eps));
    return normalize(vec3(hL - hR, 2.0 * eps * noiseScale, hD - hU));
}

void main()
{
    // Bilinear interpolation of UV coordinates across the quad patch
    vec2 uv0 = mix(tcTexCoord[0], tcTexCoord[1], gl_TessCoord.x);
    vec2 uv1 = mix(tcTexCoord[3], tcTexCoord[2], gl_TessCoord.x);
    vec2 uv  = mix(uv0, uv1, gl_TessCoord.y);

    teTexCoord = uv;

    // Sample height
    float h = terrainHeight(uv);
    teHeight = h;

    // Build world-space position: map uv [0,1] -> xz [-1,1]
    vec3 localPos = vec3(uv.x * 2.0 - 1.0, h * heightScale, uv.y * 2.0 - 1.0);

    vec4 worldPos = model * vec4(localPos, 1.0);
    teWorldPos = worldPos.xyz;

    // Normal (in local space, then rotate by model)
    float eps = 1.0 / (noiseScale * 256.0);
    vec3 localNormal = computeNormal(uv, eps);
    teNormal = normalize(mat3(transpose(inverse(model))) * localNormal);

    gl_Position = projection * view * worldPos;
}
