/*
 * From: https://takinginitiative.wordpress.com/2011/05/15/directx10-tutorial-10-shadow-mapping/
 * Simple VertexShader needed to populate the depth stencil used
 * for shadow mapping.
 */
#include "shader_common.h"

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
	float sm_aspect_ratio, sm_unused0, sm_unused1, sm_unused2;
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
	float w = 1.0 / input.pos.w;
	float3 temp;
	float Z;
	//output.pos = mul(lightWorldMatrix, input.pos);
	//output.pos = mul(lightViewProj, output.pos);

	/*
	// Regular vertex shader
	output.pos.xy = (input.pos.xy * vpScale.xy + float2(-1.0, 1.0)) * vpScale.z;
	output.pos.z = input.pos.z;
	output.pos.w = 1.0f;
	*/

	// Back-project into 3D space (this code comes from the regular vertex shader
	temp.xy = input.pos.xy * vpScale.xy + float2(-1.0, 1.0);
	temp.xy *= vpScale.z * float2(aspect_ratio, 1);
	temp.z = METRIC_SCALE_FACTOR * w;
	float4 P = float4(temp.z * temp.xy, temp.z, 1.0);
	
	// Transform this point and project it from the light's point of view:
	P = mul(lightWorldMatrix, P);
	output.pos.xy = P.xy / P.z;
	//output.pos = mul(lightViewProj, float4(P, 1));
	// The way the depth buffer and testing is setup 1.0 is Z Near, 0.0 is Z Far.
	// In this case Z Far is set at 25 meters away:
	output.pos.z = lerp(1.0, 0.0, P.z / SM_Z_FAR);
	output.pos.w = 1.0;

	return output;
}