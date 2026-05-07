#version 410 core

in vec3 teWorldPos;
in vec3 teNormal;
in vec2 teTexCoord;
in vec4 teClipSpacePos;

out vec4 FragColor;

uniform vec3 lightDir;
uniform vec3 cameraPos;
uniform vec3 oceanShallowColor;
uniform vec3 oceanDeepColor;
uniform float oceanAlpha;
uniform float oceanFresnelStrength;
uniform float oceanDistortionStrength;
uniform float oceanDepthRange;
uniform sampler2D reflectionTexture;
uniform sampler2D refractionTexture;
uniform sampler2D refractionDepthTexture;

vec3 toneMapAndGamma(vec3 color)
{
    color = color / (color + vec3(1.0));
    return pow(color, vec3(1.0 / 2.2));
}

float linearizeDepth(float depthSample, float nearPlane, float farPlane)
{
    float z = depthSample * 2.0 - 1.0;
    return (2.0 * nearPlane * farPlane) / (farPlane + nearPlane - z * (farPlane - nearPlane));
}

void main()
{
    vec2 screenUV = teClipSpacePos.xy / teClipSpacePos.w * 0.5 + 0.5;
    vec3 N = normalize(teNormal);
    vec3 V = normalize(cameraPos - teWorldPos);
    vec3 L = normalize(-lightDir);
    vec3 H = normalize(L + V);

    vec2 distortion = N.xz * oceanDistortionStrength;
    vec4 reflection = texture(reflectionTexture, screenUV + distortion);
    vec4 refraction = texture(refractionTexture, screenUV - distortion * 0.5);

    float sceneDepth = linearizeDepth(texture(refractionDepthTexture, screenUV).r, 0.05, 500.0);
    float waterSurfaceDepth = linearizeDepth(gl_FragCoord.z, 0.05, 500.0);
    float waterColumnDepth = max(sceneDepth - waterSurfaceDepth, 0.0);
    float depthBlend = clamp(waterColumnDepth / max(oceanDepthRange, 0.001), 0.0, 1.0);

    float fresnel = pow(1.0 - max(dot(N, V), 0.0), 4.0);
    fresnel = clamp(0.08 + fresnel * (0.92 * oceanFresnelStrength), 0.08, 1.0);

    vec3 depthTint = mix(oceanShallowColor, oceanDeepColor, depthBlend);
    vec3 refractedColor = mix(refraction.rgb, depthTint, 0.55 + depthBlend * 0.25);
    vec3 skyReflection = vec3(0.53, 0.73, 0.94);
    vec3 reflectedColor = mix(reflection.rgb, skyReflection, 0.12);
    vec3 color = mix(refractedColor, reflectedColor, fresnel);

    float diffuse = max(dot(N, L), 0.0);
    float specular = pow(max(dot(N, H), 0.0), 160.0) * 1.20;
    vec3 foam = vec3(0.95, 0.98, 1.0) * smoothstep(0.72, 1.0, fresnel);

    color += depthTint * (0.22 + 0.30 * diffuse);
    color += vec3(1.0, 0.98, 0.94) * specular;
    color += foam * 0.12;

    FragColor = vec4(toneMapAndGamma(color), oceanAlpha);
}
