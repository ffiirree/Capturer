#version 440

layout(location = 0) in vec2 texCoord;
layout(location = 0) out vec4 fragColor;

layout(binding = 1) uniform sampler2D plane0;

void main()
{
    fragColor = texture(plane0, texCoord).abgr;
}
