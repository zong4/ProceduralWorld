#version 410 core

in float vStrength;

out vec4 FragColor;

uniform vec4 lineColor;

void main()
{
    float alpha = lineColor.a * mix(0.45, 1.0, clamp(vStrength, 0.0, 1.0));
    vec3 color = lineColor.rgb * mix(0.72, 1.18, clamp(vStrength, 0.0, 1.0));
    FragColor = vec4(color, alpha);
}
