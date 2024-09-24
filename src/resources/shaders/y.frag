#version 440

layout(location = 0) in vec2 texCoord;
layout(location = 0) out vec4 fragColor;

layout(binding = 1) uniform sampler2D plane0;

void main()
{
    float y = texture(plane0, texCoord).r;

    fragColor = vec4(y, y, y, 1.0);
}
