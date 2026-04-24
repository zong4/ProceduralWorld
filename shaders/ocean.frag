#version 410 core

in vec3 teWorldPos;
in vec3 teNormal;
in vec2 teTexCoord;

out vec4 FragColor;

uniform vec3 lightDir;
uniform vec3 cameraPos;
uniform vec3 oceanShallowColor;
uniform vec3 oceanDeepColor;
uniform float oceanAlpha;
uniform float oceanFresnelStrength;

vec3 toneMapAndGamma(vec3 color)
{
    color = color / (color + vec3(1.0));
    return pow(color, vec3(1.0 / 2.2));
}

void main()
{
    vec3 N = normalize(teNormal);
    vec3 V = normalize(cameraPos - teWorldPos);
    vec3 L = normalize(-lightDir);
    vec3 H = normalize(L + V);

    float fresnel = pow(1.0 - max(dot(N, V), 0.0), 5.0);
    float diffuse = max(dot(N, L), 0.0);
    float specular = pow(max(dot(N, H), 0.0), 96.0) * 0.75;

    vec3 skyReflection = vec3(0.53, 0.73, 0.94);
    vec3 waterBody = mix(oceanDeepColor, oceanShallowColor, diffuse * 0.5 + 0.5);
    vec3 color = waterBody * (0.20 + diffuse * 0.35);
    color += skyReflection * fresnel * oceanFresnelStrength;
    color += vec3(1.0, 0.97, 0.92) * specular;

    FragColor = vec4(toneMapAndGamma(color), oceanAlpha);
}
