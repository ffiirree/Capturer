#version 440
#extension GL_GOOGLE_include_directive: enable

#include "uniformbuffer.glsl"
#include "colortransfer.glsl"
#include "colorconvert.glsl"
#include "tonemap.glsl"

layout (location = 0) in vec2 texCoord;
layout (location = 0) out vec4 fragColor;

layout (binding = 1) uniform sampler2D plane0;
layout (binding = 2) uniform sampler2D plane1;
layout (binding = 3) uniform sampler2D plane2;

const float SDR_PEAK_LUMINANCE = 100.0f;
const float PQ_PEAK_LUMINANCE = 10000.0f;

void main()
{
    // 1. BT.2020, no-linear: YUV -> RBG
    float Y = texture(plane0, texCoord).r * 64;
    float U = texture(plane1, texCoord).r * 64;
    float V = texture(plane2, texCoord).r * 64;

    fragColor = clamp(ubuf.M * vec4(Y, U, V, 1.0), 0.0, 1.0);

    // 2. BT.2020: no-linear -> linear
    fragColor = oetf_inverse_pq(fragColor) * PQ_PEAK_LUMINANCE / SDR_PEAK_LUMINANCE;

    // 3. BT.2020 -> BT.709
    fragColor = bt2020_to_bt709(fragColor);

    // 4. tonemap: hable
    float original = max(max(max(fragColor.r, fragColor.g), fragColor.b), 1e-6);
    float signal = tonemap_hable(original) / tonemap_hable(SDR_PEAK_LUMINANCE);

    fragColor *= signal / original;

    // 5. BT.709: linear -> no-linear
    fragColor = oetf_bt709(fragColor);
}
