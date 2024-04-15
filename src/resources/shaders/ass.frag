#version 440
#extension GL_GOOGLE_include_directive : enable

#include "uniformbuffer.glsl"

layout(location = 0) in vec2 texCoord;
layout(location = 0) out vec4 fragColor;

layout(binding = 1) uniform sampler2D plane0;

void main()
{
    float alpha = texture(plane0, texCoord).r;

    fragColor = clamp(vec4(ubuf.color.xyz, alpha), 0.0, 1.0);
}
