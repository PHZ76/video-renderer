Texture2D RGBTexture       : register(t0);

SamplerState PointSampler  : register(s0);

cbuffer Image : register(b0)
{
    float width;
    float height;
};

struct PixelShaderInput
{
    float4 pos   : SV_POSITION;
    float2 uv    : TEXCOORD0;
    float4 color : COLOR0;
};

struct PixelShaderOutput
{
    float4  YColor   : SV_Target0;
    float4  UVClolor : SV_Target1;
};

PixelShaderOutput main(PixelShaderInput input) : SV_TARGET
{
    PixelShaderOutput Output;

    const float3 offset = float3( 0.062,  0.501,  0.501);
    const float3 YCoeff = float3( 0.257,  0.504,  0.098);
    const float3 UCoeff = float3(-0.148, -0.291,  0.439);
    const float3 VCoeff = float3( 0.439, -0.368, -0.071);

    uint2  pos = uint2(input.uv * float2(width, height));

    // YUV420 - B1:Y444(x,y)
    float3 point1 = RGBTexture.Sample(PointSampler, input.uv).rgb;
    float  y = dot(point1, YCoeff) + offset.x;

    // YUV420 - B2:U444(2x,2y)  B3:V444(2x,2y)
    float2 image_uv = float2((pos.x * 2) % width / width, (pos.y * 2) % height / height);
    float3 point2 = RGBTexture.Sample(PointSampler, image_uv).rgb;
    float  u = dot(point2, UCoeff) + offset.y;
    float  v = dot(point2, VCoeff) + offset.z;

    Output.YColor   = float4(y, 0, 0, 0);
    Output.UVClolor = float4(u, v, 0, 0);
    return Output;
}