// SDL shader, https://github.com/libsdl-org/SDL/blob/main/src/render/direct3d11/SDL_shaders_d3d11.c

Texture2D YTexture : register(t0);
Texture2D UTexture : register(t1);
Texture2D VTexture : register(t2);

SamplerState LinearSampler : register(s0);
SamplerState PointSampler  : register(s1);

struct PixelShaderInput
{
    float4 pos   : SV_POSITION;
    float2 tex   : TEXCOORD0;
    float4 color : COLOR0;
};

float4 main(PixelShaderInput input) : SV_TARGET
{
    const float3 offset = {-0.0627451017, -0.501960814, -0.501960814};
    const float3 Rcoeff = {1.1644,  0.0000,  1.7927};
    const float3 Gcoeff = {1.1644, -0.2132, -0.5329};
    const float3 Bcoeff = {1.1644,  2.1124,  0.0000};

    float4 Output;

    float3 yuv;
    yuv.x = YTexture.Sample(LinearSampler, input.tex).r;
    yuv.y = UTexture.Sample(LinearSampler, input.tex).r;
    yuv.z = VTexture.Sample(LinearSampler, input.tex).r;

    yuv += offset;
    Output.r = dot(yuv, Rcoeff);
    Output.g = dot(yuv, Gcoeff);
    Output.b = dot(yuv, Bcoeff);
    Output.a = 1.0f;

    return Output;
}