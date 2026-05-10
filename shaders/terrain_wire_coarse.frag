#version 410 core

in vec2 teTexCoord;
in float teSurfaceHeight;
in float teSkirt;

out vec4 FragColor;

uniform float gridCount;
uniform float coarseLineWidth;
uniform float seaLevelOffset;

float gridLineMask(float coord, float segments, float width)
{
    float scaled = coord * max(segments, 1.0);
    float distToLine = min(fract(scaled), 1.0 - fract(scaled));
    float aa = max(fwidth(scaled) * width, 1e-4);
    return 1.0 - smoothstep(0.0, aa, distToLine);
}

void main()
{
    if (teSkirt > 0.15 || teSurfaceHeight <= seaLevelOffset + 0.0005) {
        discard;
    }

    float coarseU = gridLineMask(teTexCoord.x, gridCount, coarseLineWidth);
    float coarseV = gridLineMask(teTexCoord.y, gridCount, coarseLineWidth);
    float coarseMask = max(coarseU, coarseV);
    float alpha = clamp(coarseMask * 0.9, 0.0, 0.9);

    if (alpha < 0.01) {
        discard;
    }

    FragColor = vec4(0.08, 0.18, 0.42, alpha);
}
