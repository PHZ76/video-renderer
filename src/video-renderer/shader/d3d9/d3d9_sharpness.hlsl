// libretro shader, https://github.com/libretro/common-shaders/blob/master/test/lab/misc/sharpness.cg
// mpv shader, https://github.com/mpv-player/mpv/blob/master/video/out/gpu/video_shaders.c

texture ImageTexture;

sampler ImageSampler = sampler_state
{
    Texture   = <ImageTexture>;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
    MipFilter = NONE;
    AddressU  = WRAP;
    AddressV  = WRAP;
};

uniform float width     : register(c0);
uniform float height    : register(c1);
uniform float sharpness : register(c2);

struct PixelShaderInput
{
	float4 pos   : POSITION;
	float2 uv    : TEXCOORD0;
    float4 color : COLOR0;
};

// sharpness :0.0 to 10.0
float4 main(PixelShaderInput input) : SV_Target
{
    float2 ps = float2(1.0 / width, 1.0 / height);
    float2 st1 = ps * 1.2;
    float4 p = tex2D(ImageSampler, input.uv);
    float4 sum1 = tex2D(ImageSampler, input.uv + st1 * float2(+1, +1))
                + tex2D(ImageSampler, input.uv + st1 * float2(+1, -1))
                + tex2D(ImageSampler, input.uv + st1 * float2(-1, +1))
                + tex2D(ImageSampler, input.uv + st1 * float2(-1, -1));
    float2 st2 = ps * 1.5;
    float4 sum2 = tex2D(ImageSampler, input.uv + st2 * float2(+1,  0))
                + tex2D(ImageSampler, input.uv + st2 * float2( 0, +1))
                + tex2D(ImageSampler, input.uv + st2 * float2(-1,  0))
                + tex2D(ImageSampler, input.uv + st2 * float2( 0, -1));
    float4 t = p * 0.859375 + sum2 * -0.1171875 + sum1 * -0.09765625;
    return float4(p + t * sharpness);
}

// sharpness :0.0 to 1.0
//float4 main(PixelShaderInput input) : COLOR
//{
//    float2 ps = float2(1.0 / width, 1.0 / height);
//    float  dx = ps.x;
//    float  dy = ps.y;
// 
//    float4 t1 = input.uv.xxxy + float4(-dx, 0, dx, -dy); // A  B  C
//    float4 t2 = input.uv.xxxy + float4(-dx, 0, dx, 0);   // D  E  F
//    float4 t3 = input.uv.xxxy + float4(-dx, 0, dx, dy);  // G  H  I
//
//    float4 E = tex2D(ImageSampler, input.uv);
//    float4 color = 8 * E;
//    float4 B = tex2D(ImageSampler, t1.yw);
//    float4 D = tex2D(ImageSampler, t2.xw);
//    float4 F = tex2D(ImageSampler, t2.zw);
//    float4 H = tex2D(ImageSampler, t3.yw);
//
//    color -= tex2D(ImageSampler, t1.xw);
//    color -= B;
//    color -= tex2D(ImageSampler, t1.zw);
//    color -= D;
//    color -= F;
//    color -= tex2D(ImageSampler, t3.xw);
//    color -= H;
//    color -= tex2D(ImageSampler, t3.zw);
//
//    color = ((E != F && E != D) || (E != B && E != H)) ? saturate(E + color * sharpness) : E;
//    return color;
//}
