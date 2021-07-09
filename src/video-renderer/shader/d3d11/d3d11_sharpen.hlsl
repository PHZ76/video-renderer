// mpv shader, https://github.com/mpv-player/mpv/blob/master/video/out/gpu/video_shaders.c

Texture2D Texture : register(t0);

SamplerState LinearSampler : register(s0);
SamplerState PointSampler  : register(s1);

cbuffer SharpenParams : register(b0)
{
    float width;
    float height;
    float unsharp;
};

struct PixelShaderInput
{
    float4 pos   : SV_POSITION;
    float2 uv    : TEXCOORD0;
    float4 color : COLOR0;
};

// unsharp :0.0 to 10.0
float4 main(PixelShaderInput input) : SV_Target
{
    float2 ps = float2(1.0 / width, 1.0 / height);
    float2 st1 = ps * 1.2;
    float4 p = Texture.Sample(PointSampler, input.uv);
    float4 sum1 = Texture.Sample(PointSampler, input.uv + st1 * float2(+1, +1))
                + Texture.Sample(PointSampler, input.uv + st1 * float2(+1, -1))
                + Texture.Sample(PointSampler, input.uv + st1 * float2(-1, +1))
                + Texture.Sample(PointSampler, input.uv + st1 * float2(-1, -1));
    float2 st2 = ps * 1.5;
    float4 sum2 = Texture.Sample(PointSampler, input.uv + st2 * float2(+1,  0))
                + Texture.Sample(PointSampler, input.uv + st2 * float2(0, +1))
                + Texture.Sample(PointSampler, input.uv + st2 * float2(-1,  0))
                + Texture.Sample(PointSampler, input.uv + st2 * float2(0, -1));
    float4 t = p * 0.859375 + sum2 * -0.1171875 + sum1 * -0.09765625;
    return float4(p + t * unsharp);
}
