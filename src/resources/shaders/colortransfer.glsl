#ifndef COLOR_TRANSFER_GLSL
#define COLOR_TRANSFER_GLSL

// https://github.com/sekrit-twc/zimg/blob/master/src/zimg/colorspace/gamma.cpp

const float ST2084_OOTF_SCALE = 59.49080238715383f;

vec4 ootf_1_2(vec4 rgba)
{
    return mix(pow(rgba, vec4(1.2f)), rgba, lessThan(rgba, vec4(0.0f)));
}

vec4 ootf_inverse_1_2(vec4 rgba)
{
    return mix(pow(rgba, vec4(1.0f / 1.2f)), rgba, lessThan(rgba, vec4(0.0f)));
}

vec4 eotf_bt1886(vec4 rgba)
{
    return mix(pow(rgba, vec4(2.4f)), rgba, lessThan(rgba, vec4(0.0f)));
}

vec4 eotf_inverse_bt1886(vec4 rgba)
{
    return mix(pow(rgba, vec4(1.0f / 2.4f)), rgba, lessThan(rgba, vec4(0.0f)));
}

// BT.709

vec4 oetf_bt709(vec4 rgba)
{
    const vec4 alpha = vec4(vec3(1.09929682680944f), 1.0);
    const vec4 beta = vec4(0.018053968510807f);

    vec4 L = 4.5 * rgba;
    vec4 H = alpha * pow(rgba, vec4(0.45)) - (alpha - 1.0);

    return mix(H, L, lessThan(rgba, beta));
}

vec4 oetf_inverse_bt709(vec4 rgba)
{
    const vec4 alpha = vec4(vec3(1.09929682680944f), 1.0);
    const vec4 beta = 4.5 * vec4(0.018053968510807f);

    vec4 L = rgba / 4.5;
    vec4 H = pow((rgba + alpha - 1.0) / alpha, vec4(1. / 0.45));

    return mix(H, L, lessThan(rgba, beta));
}

// sRGB
const float SRGB_ALPHA = 1.055010718947587f;
const float SRGB_BETA = 0.003041282560128f;

vec4 eotf_srgb(vec4 sRGB)
{
    const vec3 alpha = vec3(1.055010718947587f);
    const vec3 beta = vec3(12.92 * 0.003041282560128f);

    vec3 L = sRGB.rgb / 12.92f;
    vec3 H = pow((sRGB.rgb + (alpha - 1.0)) / alpha, vec3(2.4f));

    return vec4(mix(H, L, lessThanEqual(sRGB.rgb, beta)), sRGB.a);
}

vec4 eotf_inverse_srgb(vec4 linear)
{
    const vec3 alpha = vec3(1.055010718947587f);
    const vec3 beta = vec3(0.003041282560128f);

    vec3 L = linear.rgb * 12.92;
    vec3 H = alpha * pow(linear.rgb, vec3(1.0 / 2.4f)) - (alpha - 1.0f);

    return vec4(mix(H, L, lessThanEqual(linear.rgb, beta)), linear.a);
}

// AVCOL_TRC_SMPTE2084 (PQ)

// Tonemapping into the RGB range supported by the output is done using
// https://www.itu.int/dms_pub/itu-r/opb/rep/R-REP-BT.2390-6-2019-PDF-E.pdf, section 5.4
//
const float ST2084_M1 = 0.1593017578125f;
const float ST2084_M2 = 78.84375f;
const float ST2084_C1 = 0.8359375f;
const float ST2084_C2 = 18.8515625f;
const float ST2084_C3 = 18.6875f;

vec4 eotf_pq(vec4 rgba)
{
    vec4 e = pow(rgba, 1.0f / vec4(ST2084_M2));
    vec4 num = max(e - ST2084_C1, 0.0f);
    vec4 den = ST2084_C2 - ST2084_C3 * e;

    return pow(num / den, 1.0f / vec4(ST2084_M1));
}

float eotf_pq(float r)
{
    float e = pow(r, 1.0f / ST2084_M2);
    float num = max(e - ST2084_C1, 0.0f);
    float den = ST2084_C2 - ST2084_C3 * e;

    return pow(num / den, 1.0f / ST2084_M1);
}

vec4 eotf_inverse_pq(vec4 rgba)
{
    vec4 p = pow(rgba, vec4(ST2084_M1));
    vec4 num = (ST2084_C1 - 1.0f) + (ST2084_C2 - ST2084_C3) * p;
    vec4 den = 1.0f + ST2084_C3 * p;

    return pow(1.0f + num / den, vec4(ST2084_M2));
}

float eotf_inverse_pq(float r)
{
    float p = pow(r, ST2084_M1);
    float num = (ST2084_C1 - 1.0f) + (ST2084_C2 - ST2084_C3) * p;
    float den = 1.0f + ST2084_C3 * p;

    return pow(1.0f + num / den, ST2084_M2);
}

vec4 ootf_pq(vec4 x)
{
    return eotf_bt1886(oetf_bt709(x * ST2084_OOTF_SCALE)) / 100.0f;
}

vec4 ootf_inverse_pq(vec4 x)
{
    return oetf_inverse_bt709(eotf_inverse_bt1886(x * 100.0f)) / ST2084_OOTF_SCALE;
}

// OETF = OOTF * EOTF^{-1}
vec4 oetf_pq(vec4 rgba)
{
    return eotf_inverse_pq(ootf_pq(rgba));
}

vec4 oetf_inverse_pq(vec4 rgba)
{
    return ootf_inverse_pq(eotf_pq(rgba));
}

// ARIB STD-B67 (HLG)
const float HLG_A = 0.17883277f;
const float HLG_B = 0.28466892f;
const float HLG_C = 0.55991073f;

vec4 oetf_hlg(vec4 rgba)
{
    rgba = max(rgba, vec4(0.0f));

    vec4 L = sqrt(3.0f * rgba);
    vec4 H = HLG_A * log(12.0f * rgba - HLG_B) + HLG_C;

    return mix(H, L, lessThan(rgba, vec4(1.0f / 12.0f)));
}

vec4 oetf_inverse_hlg(vec4 rgba)
{
    rgba = max(rgba, vec4(0.0f));

    vec4 L = (rgba * rgba) / 3.0f;
    vec4 H = (exp((rgba - HLG_C) / HLG_A) + HLG_B) / 12.0f;

    return mix(H, L, lessThan(rgba, vec4(0.5)));
}

vec4 eotf_hlg(vec4 rgba)
{
    return ootf_1_2(oetf_inverse_hlg(rgba));
}

vec4 eotf_inverse_hlg(vec4 rgba)
{
    return oetf_hlg(ootf_inverse_1_2(rgba));
}

#endif
