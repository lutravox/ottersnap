#version 450

layout(location = 0) in vec2 fragUV;
layout(location = 0) out vec4 outColor;

layout(push_constant) uniform IndicatorConstants {
    vec2 offset;
    vec2 size;
    vec4 color;
} constants;

void main() {
    vec2 uv = fragUV - 0.5;
    float dist = length(uv);
    float delta = fwidth(dist);

    // Anti-aliased outer edge
    float alpha = 1.0 - smoothstep(0.5 - delta * 0.5, 0.5 + delta * 0.5, dist);

    if (alpha <= 0.0) {
        discard;
    }

    // Anti-aliased white border
    float border = smoothstep(0.35 - delta * 0.5, 0.35 + delta * 0.5, dist);

    vec4 finalColor = mix(constants.color, vec4(1.0, 1.0, 1.0, 1.0), border);

    outColor = vec4(finalColor.rgb * alpha, alpha);
}
