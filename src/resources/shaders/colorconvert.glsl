#ifndef COLOR_CONVERT_GLSL
#define COLOR_CONVERT_GLSL

vec4 bt2020_to_bt709(vec4 rgba)
{
    const mat4 M = mat4(
    1.660491f, -0.587641f, -0.072850f, 0.000000f,
    -0.124550f, 1.132900f, -0.008349f, 0.000000f,
    -0.018151f, -0.100579f, 1.118730f, 0.000000f,
    0.000000f, 0.000000f, 0.000000f, 1.000000f
    );

    return rgba * M;
}

#endif