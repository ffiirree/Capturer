#ifndef TONEMAP_GLSL
#define TONEMAP_GLSL

float tonemap_hable(float x)
{
    float a = 0.15f;
    float b = 0.50f;
    float c = 0.10f;
    float d = 0.20f;
    float e = 0.02f;
    float f = 0.30f;

    return (x * (x * a + b * c) + d * e) / (x * (x * a + b) + d * f) - e / f;
}

#endif