
cbuffer VertexShaderConstants : register(b0)
{
    matrix g_World; // matrix可以用float4x4替代。不加row_major的情况下，矩阵默认为列主矩阵，
    matrix g_View; // 可以在前面添加row_major表示行主矩阵
    matrix g_Proj; // 该教程往后将使用默认的列主矩阵，但需要在C++代码端预先将矩阵进行转置。
};

struct VertexShaderInput
{
	float3 pos   : POSITION;
	float2 uv    : TEXCOORD0;
	float4 color : COLOR0;
};

struct VertexShaderOutput
{
	float4 pos   : SV_POSITION;
	float2 uv    : TEXCOORD0;
	float4 color : COLOR0;
};

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
	//float4 pos = float4(input.pos, 1);

	//pos = mul(pos, view);
    //pos = mul(pos, projection);

    output.pos = mul(float4(input.pos, 1.0), g_World);
    output.pos = mul(output.pos, g_View);
    output.pos = mul(output.pos, g_Proj);
	
	output.uv = input.uv;
	output.color = input.color;

	return output;
}

