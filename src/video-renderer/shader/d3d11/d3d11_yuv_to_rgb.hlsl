Texture2D YUV420YTexture      : register(t0);
Texture2D YUV420UVTexture     : register(t1);
Texture2D Chroma420YTexture   : register(t2);
Texture2D Chroma420UVTexture  : register(t3);

SamplerState PointSampler     : register(s0);

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

float4 main(PixelShaderInput input) : SV_TARGET
{
	float4 Output;

	const float3 offset = float3(-0.062, -0.501, -0.501);
	const float3 Rcoeff = float3( 1.164,  0.000,  1.596);
	const float3 Gcoeff = float3( 1.164, -0.392, -0.813);
	const float3 Bcoeff = float3( 1.164,  2.017,  0.000);

	uint2 pos = uint2(input.uv * float2(width, height));
	float3 yuv444 = offset;

	// B1
	yuv444.r = YUV420YTexture.Sample(PointSampler, input.uv).r;

	if (pos.x % 2 == 0 && pos.y % 2 == 0)
	{
		// B2 B3
		yuv444.gb = YUV420UVTexture.Sample(PointSampler, input.uv).rg;
	}
	else if (pos.x % 2 == 1)
	{
		// B4 B5
		yuv444.g = Chroma420YTexture.Sample(PointSampler, float2(input.uv.x / 2, input.uv.y)).r;
		yuv444.b = Chroma420YTexture.Sample(PointSampler, float2(input.uv.x / 2 + 0.5, input.uv.y)).r;
	}
	else if (pos.x % 4 == 0 && pos.y % 2 == 1)
	{
		// B6 B7
		yuv444.g = Chroma420UVTexture.Sample(PointSampler, float2(input.uv.x / 2, input.uv.y)).r;
		yuv444.b = Chroma420UVTexture.Sample(PointSampler, float2(input.uv.x / 2 + 0.5, input.uv.y)).r;
	}
	else if (pos.x % 4 == 2 && pos.y % 2 == 1)
	{
		// B8 B9
		yuv444.g = Chroma420UVTexture.Sample(PointSampler, float2(input.uv.x / 2, input.uv.y)).g;
		yuv444.b = Chroma420UVTexture.Sample(PointSampler, float2(input.uv.x / 2 + 0.5, input.uv.y)).g;
	}

	// yuv to rgb
	yuv444 += offset;
	Output.r = dot(yuv444, Rcoeff);
	Output.g = dot(yuv444, Gcoeff);
	Output.b = dot(yuv444, Bcoeff);
	Output.a = 1.0f;
	return Output;
}