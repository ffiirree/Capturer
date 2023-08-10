struct VS_OUTPUT
{
    float2 tex : TEXCOORD;
    float4 pos : SV_POSITION;
};

cbuffer CBuf : register(b0) { matrix ProjM; };

VS_OUTPUT vs_main(float3 pos : POSITION, float2 tex : TEXCOORD)
{
    VS_OUTPUT vsout;
    vsout.pos = mul(ProjM, float4(pos, 1));
    vsout.tex = tex;
    return vsout;
}