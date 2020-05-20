/*
 * From: https://takinginitiative.wordpress.com/2011/05/15/directx10-tutorial-10-shadow-mapping/
 * Simple VertexShader needed to populate the depth stencil used
 * for shadow mapping.
 */

// VertexShaderCBuffer
cbuffer ConstantBuffer : register(b0)
{
	float4 vpScale;
	float aspect_ratio, cockpit_threshold, z_override, sz_override;
	float mult_z_override, bPreventTransform, bFullTransform, metric_mult;
};

// ShadowMapVertexShaderMatrixCB
cbuffer ConstantBuffer : register(b5)
{
	matrix lightViewProj;
	matrix lightWorldMatrix;
};

struct VertexShaderInput
{
	float4 pos      : POSITION;
	float4 color    : COLOR0;
	float4 specular : COLOR1;
	float2 tex      : TEXCOORD;
};

struct SHADOW_PS_INPUT
{
	float4 pos : SV_POSITION;
};

//--------------------------------------------------------------------------------------
// Vertex Shader
//--------------------------------------------------------------------------------------
SHADOW_PS_INPUT main(VertexShaderInput input)
{
	SHADOW_PS_INPUT output;
	//output.pos = mul(lightWorldMatrix, input.pos);
	//output.pos = mul(lightViewProj, output.pos);

	// Regular vertex shader
	output.pos.xy = (input.pos.xy * vpScale.xy + float2(-1.0, 1.0)) * vpScale.z;
	output.pos.z = input.pos.z;
	output.pos.w = 1.0f;

	return output;
}