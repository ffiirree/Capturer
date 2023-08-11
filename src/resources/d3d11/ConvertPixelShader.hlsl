Texture2D<float> Y : t0;
Texture2D<float2> UV : t1;

SamplerState splr;

static const float3x3 BT709M =
{
    1.1643835616f, +1.1643835616f, 1.1643835616f,
    0.0000000000f, -0.2132486143f, 2.1124017857f,
    1.7927410714f, -0.5329093286f, 0.0000000000f
};

static const float3 OFFSET = { -0.0627451017, -0.501960814, -0.501960814 };

float4 ps_main(float2 tex : TEXCOORD) : SV_TARGET
{
    float3 yuv = float3(Y.Sample(splr, tex), UV.Sample(splr, tex));

    return float4(mul(yuv + OFFSET, BT709M), 1.0);
}