struct VS_INPUT
{
    float4 Pos : POSITION;
    float2 Tex : TEXCOORD;
};

struct VS_OUTPUT
{
    float4 Pos : SV_POSITION;
    float2 Tex : TEXCOORD;
};

cbuffer CONSNAT_BUFFER : register(b0) { matrix ProjM; };

//--------------------------------------------------------------------------------------
// Vertex Shader
//--------------------------------------------------------------------------------------
VS_OUTPUT vs_main(VS_INPUT input)
{
    VS_OUTPUT output;
    output.Pos = mul(ProjM, input.Pos);
    output.Tex = input.Tex;
    return output;
}