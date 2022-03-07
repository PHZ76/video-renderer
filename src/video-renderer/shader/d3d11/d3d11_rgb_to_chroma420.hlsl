Texture2D RGBTexture      : register(t0);

SamplerState PointSampler : register(s0);

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

PixelShaderOutput main(PixelShaderInput input)
{
    PixelShaderOutput Output;

    const float  offset = 0.501;
    const float3 YCoeff = float3( 0.257,  0.504,  0.098);
    const float3 UCoeff = float3(-0.148, -0.291,  0.439);
    const float3 VCoeff = float3( 0.439, -0.368, -0.071);
    
    uint2  pos = uint2(input.uv * float2(width, height));
    uint2  max_pos = uint2(0, 0);
    float3 coeff = UCoeff;
    float  y = 0;
    float  u = 0;
    float  v = 0;

    // Chroma420 - B4:U444(2x+1, y)  B5:V444(2x+1, y)
    max_pos.x = width / 2;
	if (pos.x < max_pos.x)
	{
		// B4
        coeff = UCoeff;
	}
    else
    {
        // B5
        coeff = VCoeff;
    }

    // B4  B5
    float2 image_uv  = float2(((pos.x % (width / 2)) * 2 + 1) / width, pos.y / height);
    float3 image_rgb = RGBTexture.Sample(PointSampler, image_uv).rgb;
    y = dot(image_rgb, coeff) + offset;

    // Chroma420 - B6:U444(4x,2y+1) B7:V444(4x,2y+1) B8:U444(4x+2,2y+1) B9:V444(4x+2,2y+1)
    max_pos.x = width / 2;
    max_pos.y = height / 2;
    if ((pos.x < max_pos.x) && (pos.y < max_pos.y))
    {
        max_pos.x = width / 4;
        if (pos.x < max_pos.x)
        {
            // B6 B8
            coeff = UCoeff;
        }
        else
        {
            // B7 B9
            coeff = VCoeff;
        }

        // B6 B7
        image_uv = float2(((pos.x % (width / 4)) * 4) / width, ((pos.y%(height/2)) * 2 + 1) / height);
        image_rgb = RGBTexture.Sample(PointSampler, image_uv).rgb;
        u = dot(image_rgb, coeff) + offset;

        // B8 B9
        image_uv = float2(((pos.x % (width / 4)) * 4 + 2) / width, ((pos.y % (height / 2)) * 2 + 1) / height);
        image_rgb = RGBTexture.Sample(PointSampler, image_uv).rgb;
        v = dot(image_rgb, coeff) + offset;
    }

    Output.YColor   = float4(y, 0, 0, 0);
    Output.UVClolor = float4(u, v, 0, 0);
    return Output;
}