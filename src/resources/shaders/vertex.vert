#version 440
#extension GL_GOOGLE_include_directive: enable

#include "uniformbuffer.glsl"

layout (location = 0) in vec4 position;
layout (location = 1) in vec2 tex;

layout (location = 0) out vec2 texCoord;

void main()
{
    texCoord = tex;
    gl_Position = ubuf.mvp * position;
}
