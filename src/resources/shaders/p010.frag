#version 440
#extension GL_GOOGLE_include_directive: enable

#include "uniformbuffer.glsl"

layout (location = 0) in vec2 texCoord;
layout (location = 0) out vec4 fragColor;

layout (binding = 1) uniform sampler2D plane0;
layout (binding = 2) uniform sampler2D plane1;

void main()
{
    float Y = texture(plane0, texCoord).r * 64;
    float U = texture(plane1, texCoord).r * 64;
    float V = texture(plane1, texCoord).g * 64;

    fragColor = clamp(ubuf.M * vec4(Y, U, V, 1.0), 0.0, 1.0);
}
