#version 450

layout(location = 0) in vec2 inPos;
layout(location = 0) out vec2 fragUV;

layout(push_constant) uniform IndicatorConstants {
    vec2 offset;
    vec2 size;
    vec4 color;
} constants;

void main() {
    fragUV = inPos * 0.5 + 0.5;
    vec2 pos = inPos * constants.size + constants.offset;
    gl_Position = vec4(pos, 0.0, 1.0);
}
