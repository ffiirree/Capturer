#version 440
#extension GL_GOOGLE_include_directive : enable

#include "uniformbuffer.glsl"

layout(location = 0) in vec2 texCoord;
layout(location = 0) out vec4 fragColor;

layout(binding = 1) uniform sampler2D plane0;

void main()
{
    // | x  | y  | z  | w  |
    // | Y0 | U0 | Y1 | V0 |
    // FIXME:
    float y = texture(plane0, texCoord).x;
    float u = texture(plane0, texCoord).y;
    float v = texture(plane0, texCoord).w;

    fragColor = clamp(ubuf.M * vec4(y, u, v, 1.0), 0.0, 1.0);
}
