// SDL shader, https://github.com/libsdl-org/SDL/blob/main/src/render/direct3d/SDL_shaders_d3d.c

texture YTexture;
texture UTexture;
texture VTexture;

sampler YSampler = sampler_state
{
    Texture   = <YTexture>;
    magfilter = LINEAR;
    minfilter = LINEAR;
    mipfilter = POINT;
    AddressU  = CLAMP;
    AddressV  = CLAMP;
};

sampler USampler = sampler_state
{
    Texture   = <UTexture>;
    magfilter = LINEAR;
    minfilter = LINEAR;
    mipfilter = POINT;
    AddressU  = CLAMP;
    AddressV  = CLAMP;
};

sampler VSampler = sampler_state
{
    Texture   = <VTexture>;
    magfilter = LINEAR;
    minfilter = LINEAR;
    mipfilter = POINT;
    AddressU  = CLAMP;
    AddressV  = CLAMP;
};

struct PixelShaderInput
{
    float4 pos   : POSITION;
    float2 uv    : TEXCOORD0;
    float4 color : COLOR0;
};

float4 main(PixelShaderInput input) : COLOR
{
    const float3 offset = {-0.0627451017, -0.501960814, -0.501960814};
    const float3 Rcoeff = {1.1644,  0.0000,  1.7927};
    const float3 Gcoeff = {1.1644, -0.2132, -0.5329};
    const float3 Bcoeff = {1.1644,  2.1124,  0.0000};

    float4 Output;

    float3 yuv;
    yuv.x = tex2D(YSampler, input.uv).r;
    yuv.y = tex2D(USampler, input.uv).r;
    yuv.z = tex2D(VSampler, input.uv).r;

    yuv += offset;
    Output.r = dot(yuv, Rcoeff);
    Output.g = dot(yuv, Gcoeff);
    Output.b = dot(yuv, Bcoeff);
    Output.a = 1.0f;

    return Output;
}