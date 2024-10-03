#version 440
#extension GL_GOOGLE_include_directive: enable

#include "uniformbuffer.glsl"
#include "colortransfer.glsl"
#include "tonemap.glsl"

layout (location = 0) in vec2 texCoord;
layout (location = 0) out vec4 fragColor;

layout (binding = 1) uniform sampler2D plane0;
layout (binding = 2) uniform sampler2D plane1;
layout (binding = 3) uniform sampler2D plane2;

const float SDR_PEAK_LUMINANCE = 100.0f;
const float PQ_PEAK_LUMINANCE = 10000.0f;
const float HLG_PEAK_LUMINANCE = 10000.0f;
const mat4 BT2020_2_BT709 = mat4(
+ 1.660491f, -0.587641f, -0.072850f, 0.000000f,
-0.124550f, + 1.132900f, -0.008349f, 0.000000f,
-0.018151f, -0.100579f, + 1.118730f, 0.000000f,
+ 0.000000f, + 0.000000f, + 0.000000f, 1.000000f
);

void main()
{
    float Y = texture(plane0, texCoord).r * 64;
    float U = texture(plane1, texCoord).r * 64;
    float V = texture(plane2, texCoord).r * 64;

    fragColor = clamp(ubuf.M * vec4(Y, U, V, 1.0), 0.0, 1.0);

#ifdef PQ_HDR2SDR
    fragColor = oetf_inverse_pq(fragColor) * PQ_PEAK_LUMINANCE / SDR_PEAK_LUMINANCE;
    fragColor = fragColor * BT2020_2_BT709;

    float original = max(max(max(fragColor.r, fragColor.g), fragColor.b), 1e-6);
    float signal = tonemap_hable(original) / tonemap_hable(SDR_PEAK_LUMINANCE / 10.0f);

    fragColor *= signal / original;

    fragColor = oetf_bt709(fragColor);
#endif
}
