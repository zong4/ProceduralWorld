#version 410 core

in vec3 teWorldPos;
in vec3 teNormal;
in vec2 teTexCoord;
in float teHeight;

out vec4 FragColor;

uniform vec3 lightDir;
uniform vec3 cameraPos;
uniform float time;
uniform int renderMode;    // 0=shaded, 1=wireframe overlay, 2=height map, 3=normals, 4=tessellation debug

// Biome coloring based on height and slope
vec3 terrainColor(float height, vec3 normal, vec3 worldPos)
{
    float slope = 1.0 - normal.y;  // 0 = flat, 1 = vertical

    // Colors
    vec3 deepWater   = vec3(0.05, 0.12, 0.28);
    vec3 shallowWater= vec3(0.08, 0.25, 0.45);
    vec3 sand        = vec3(0.76, 0.70, 0.50);
    vec3 grass       = vec3(0.25, 0.50, 0.18);
    vec3 darkGrass   = vec3(0.15, 0.35, 0.10);
    vec3 rock        = vec3(0.45, 0.40, 0.35);
    vec3 darkRock    = vec3(0.25, 0.22, 0.18);
    vec3 snow        = vec3(0.92, 0.95, 1.00);

    // Normalize height to [0,1] range (height is roughly -0.5..0.5 from noise)
    float h = height * 0.5 + 0.5;

    vec3 col;

    if (h < 0.30) {
        // Water
        float t = smoothstep(0.0, 0.30, h);
        col = mix(deepWater, shallowWater, t);
    } else if (h < 0.36) {
        // Beach / sand
        float t = smoothstep(0.30, 0.36, h);
        col = mix(shallowWater, sand, t);
    } else if (h < 0.55) {
        // Grass / lowland
        float t = smoothstep(0.36, 0.55, h);
        col = mix(grass, darkGrass, t);
    } else if (h < 0.75) {
        // Rocky highlands
        float t = smoothstep(0.55, 0.75, h);
        col = mix(darkGrass, rock, t);
    } else if (h < 0.88) {
        float t = smoothstep(0.75, 0.88, h);
        col = mix(rock, darkRock, t);
    } else {
        // Snow cap
        float t = smoothstep(0.88, 1.0, h);
        col = mix(darkRock, snow, t);
    }

    // Steep slopes -> rock override
    if (slope > 0.45) {
        float t = smoothstep(0.45, 0.70, slope);
        col = mix(col, darkRock, t);
    }

    return col;
}

void main()
{
    if (renderMode == 3) {
        // Normal visualization
        FragColor = vec4(teNormal * 0.5 + 0.5, 1.0);
        return;
    }
    if (renderMode == 2) {
        // Height map grayscale
        float h = teHeight * 0.5 + 0.5;
        FragColor = vec4(vec3(h), 1.0);
        return;
    }

    // --- Lighting ---
    vec3 N = normalize(teNormal);
    vec3 L = normalize(-lightDir);
    vec3 V = normalize(cameraPos - teWorldPos);
    vec3 H = normalize(L + V);

    float ambient  = 0.18;
    float diffuse  = max(dot(N, L), 0.0);

    // Soft self-shadow via slope factor
    float shadow   = mix(1.0, 0.6, clamp(1.0 - N.y, 0.0, 1.0));

    // Specular (very subtle on terrain)
    float specular = pow(max(dot(N, H), 0.0), 32.0) * 0.08;

    // Atmospheric fog
    float fogDist  = length(cameraPos - teWorldPos);
    float fog      = exp(-fogDist * fogDist * 0.000015);

    vec3 sunColor  = vec3(1.0, 0.95, 0.85);
    vec3 skyColor  = vec3(0.53, 0.73, 0.94);
    vec3 fogColor  = skyColor;

    vec3 baseColor = terrainColor(teHeight, N, teWorldPos);

    // Hemisphere ambient (sky above, ground below)
    vec3 ambientColor = mix(vec3(0.10, 0.08, 0.06), skyColor * 0.4, N.y * 0.5 + 0.5);

    vec3 color = baseColor * (ambientColor + diffuse * sunColor * shadow) + specular * sunColor;
    color = mix(fogColor, color, fog);

    // Tone mapping (Reinhard)
    color = color / (color + vec3(1.0));
    // Gamma correction
    color = pow(color, vec3(1.0 / 2.2));

    if (renderMode == 1) {
        // Wireframe overlay mode: just output a semi-transparent tint;
        // actual wireframe is drawn separately in the app
        FragColor = vec4(color, 1.0);
        return;
    }

    FragColor = vec4(color, 1.0);
}
