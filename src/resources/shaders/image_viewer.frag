#version 450

const float EPS = 0.001;

layout(binding = 0) uniform Uniforms {
    vec2  uViewport;      // widget size in pixels
    vec2  uFitImgSize;    // fitted image size in viewport pixels
    vec2  uFitImgOrigin;  // top-left corner of fitted image in viewport pixels
    vec2  uPanOffset;     // pan offset (center-shifted UV space)
    float uFitScale;      // display pixels / image pixels (for LOD)
    float uZoomLevel;     // display scale relative to fit (1.0 = 1:1)
    bool  uGrayscale;     // true = grayscale
    bool  uMirror;        // true = mirror
} u;

layout(binding = 1) uniform sampler2D tex;

layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 fragColor;

void main()
{
    // Convert fragment UV to screen-space pixel coordinates.
    vec2 screen = vUV * u.uViewport;
    vec2 offset = screen - u.uFitImgOrigin;

    // Normalise to [0,1] image-space UV, then shift to [-0.5, 0.5].
    vec2 imgUV = offset / max(u.uFitImgSize, EPS.xx) - 0.5;

    // Mirror - flip horizontally around image center
    if (u.uMirror)
        imgUV.x = -imgUV.x;

    // UV zoom multiplier: fitScale / zoomLevel.
    // At zoomLevel=1.0 (1:1), the image is displayed at fitScale UV units
    // per screen pixel — i.e. zoomed in relative to the fitted image.
    float zoomFactor = u.uFitScale / max(u.uZoomLevel, EPS);
    imgUV *= zoomFactor;

    // Apply pan
    // Negate x when mirrored so drag direction feels natural.
    imgUV.x += (u.uMirror ? -1.0 : 1.0) * u.uPanOffset.x;
    imgUV.y += u.uPanOffset.y;

    // Shift back to [0,1] for discard check and texture sampling.
    imgUV += 0.5;

    // Discard fragments outside the image — the clear colour forms the
    // letterbox / pillarbox bars and areas revealed when zoomed in.
    if (imgUV.x < 0.0 || imgUV.y < 0.0 ||
        imgUV.x > 1.0 || imgUV.y > 1.0)
        discard;

    // Mip level from zoom: lower zoom → higher mip level.
    float lod = -log2(max(u.uZoomLevel, EPS));

    // Bilinear filtering with mipmaps
    vec4 color = textureLod(tex, imgUV, lod);

    // Checkerboard background for transparency
    float checkSize = 16.0;
    vec2 checkPos = screen / checkSize;
    float check = mod(floor(checkPos.x) + floor(checkPos.y), 2.0);
    vec3 bgColor = mix(vec3(0.8), vec3(1.0), check);

    // Blend image with background based on alpha
    color.rgb = mix(bgColor, color.rgb, color.a);
    color.a = 1.0;

    // Grayscale (ITU-R BT.709 luminance)
    if (u.uGrayscale) {
        float lum = dot(color.rgb, vec3(0.2126, 0.7152, 0.0722));
        color.rgb = vec3(lum);
    }

    fragColor = color;
}
