
cbuffer VertexShaderConstants : register(b0)
{
    matrix g_World; // matrix������float4x4���������row_major������£�����Ĭ��Ϊ��������
    matrix g_View; // ������ǰ�����row_major��ʾ��������
    matrix g_Proj; // �ý̳�����ʹ��Ĭ�ϵ��������󣬵���Ҫ��C++�����Ԥ�Ƚ��������ת�á�
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

