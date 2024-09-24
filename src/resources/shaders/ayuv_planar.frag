#version 440
#extension GL_GOOGLE_include_directive : enable

#include "uniformbuffer.glsl"

layout(location = 0) in vec2 texCoord;
layout(location = 0) out vec4 fragColor;

layout(binding = 1) uniform sampler2D plane0;
layout(binding = 2) uniform sampler2D plane1;
layout(binding = 3) uniform sampler2D plane2;
layout(binding = 4) uniform sampler2D plane3;

void main()
{
    float y = texture(plane0, texCoord).r;
    float u = texture(plane1, texCoord).r;
    float v = texture(plane2, texCoord).r;
    float a = texture(plane3, texCoord).r;

    fragColor = clamp(ubuf.M * vec4(y, u, v, a), 0.0, 1.0);
}
