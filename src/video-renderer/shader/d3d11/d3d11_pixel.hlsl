Texture2D Texture : register(t0);

SamplerState LinearSampler : register(s0);
SamplerState PointSampler  : register(s1);

struct PixelShaderInput
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

float4 main(PixelShaderInput input) : SV_TARGET
{
    return Texture.Sample(LinearSampler, input.uv);
}