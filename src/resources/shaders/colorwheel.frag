#version 450

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
};

void main() {
    vec2 uv = qt_TexCoord0 - 0.5;
    float dist = length(uv);

    float angle = atan(uv.y, uv.x);
    float hue = (angle / 6.2831853) + 0.5;
    float sat = clamp(dist * 2.0, 0.0, 1.0);

    // Robust branchless HSV to RGB conversion
    vec3 rgb = clamp(abs(mod(hue * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);

    // Mix between white (center) and the hue (edge) based on saturation
    rgb = mix(vec3(1.0), rgb, sat);

    // Anti-aliasing the outer edge
    // We use a radius of 0.49 to avoid any possible clipping at the edges of the item
    // and a wider smoothing range (0.02) for visible anti-aliasing.
    float radius = 0.49;
    float smoothing = 0.005;
    float alpha = smoothstep(radius + smoothing, radius - smoothing, dist);

    if (alpha <= 0.0) {
        discard;
    }

    float finalAlpha = alpha * qt_Opacity;
    fragColor = vec4(rgb * finalAlpha, finalAlpha);
}
