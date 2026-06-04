#version 410 core

in vec2 vUv;
out vec4 FragColor;

uniform sampler3D sourceScatteringTexture;
uniform float layerIndex;
uniform float layerCount;

void main()
{
    float z = layerCount > 1.0 ? layerIndex / (layerCount - 1.0) : 0.0;
    FragColor = texture(sourceScatteringTexture, vec3(vUv, z));
}
