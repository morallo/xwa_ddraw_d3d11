/*
 * From: https://takinginitiative.wordpress.com/2011/05/15/directx10-tutorial-10-shadow-mapping/
 * Simple VertexShader needed to populate the depth stencil used
 * for shadow mapping.
 */
#include "shader_common.h"
#include "shadow_mapping_common.h"

// VertexShaderCBuffer
cbuffer ConstantBuffer : register(b0)
{
	float4 vpScale;
	float aspect_ratio, cockpit_threshold, z_override, sz_override;
	float mult_z_override, bPreventTransform, bFullTransform, metric_mult;
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

/*

Code copied from the Blender file used to run experiments
# POV values for the X-W taken from MXvTED:
POVX =   0.0
POVY =  37.0
POVZ = -31.0

POV_FACTOR_XY = 41
POV_FACTOR_Z = 41

shy = -0.075
sz = 0.94

POVX_OFS = POVX / POV_FACTOR_XY
POVY_OFS = POVY / POV_FACTOR_XY
POVZ_OFS = POVZ / POV_FACTOR_Z

# XWA to Shadow Model transform:
for i in range(num_verts):
	p = verts[i].co.xyz
	mesh.verts[i].co.x = p.x + POVX_OFS
	mesh.verts[i].co.y = p.y + p.z * shy + POVY_OFS
	mesh.verts[i].co.z = p.z * sz + POVZ_OFS

*/

//--------------------------------------------------------------------------------------
// Vertex Shader
//--------------------------------------------------------------------------------------
// Use this shader for screen-space shadow mapping
SHADOW_PS_INPUT main_old(VertexShaderInput input)
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
	float4 P = float4(temp.z * temp.xy / DEFAULT_FOCAL_DIST, temp.z, 1.0);
	
	// Transform this point and project it from the light's point of view:
	P = mul(lightWorldMatrix, P);
	//output.pos.xy = P.xy / P.z; // Perspective projection
	output.pos.xy = P.xy; // Parallel projection
	// The way the depth buffer and testing is setup 1.0 is Z Near, 0.0 is Z Far.
	// In this case Z Far is set at SM_Z_FAR meters away:
	output.pos.z = lerp(1.0, 0.0, (P.z - SM_Z_NEAR) / SM_Z_FAR);
	output.pos.w = 1.0;

	return output;
}

// Use this shader for 3D Shadow OBJ
SHADOW_PS_INPUT main(VertexShaderInput input)
{
	SHADOW_PS_INPUT output;

	// The input should be true metric 3D
	float4 P = float4(input.pos.xyz, 1.0);

	// Transform this point and project it from the light's point of view:
	P = mul(lightWorldMatrix, P);
	//output.pos.xy = P.xy / P.z; // Perspective projection
	output.pos.xy = P.xy; // Parallel projection
	// The way the depth buffer and testing is setup 1.0 is Z Near, 0.0 is Z Far.
	// In this case Z Far is set at SM_Z_FAR meters away:
	output.pos.z = lerp(1.0, 0.0, (P.z - SM_Z_NEAR) / SM_Z_FAR);
	output.pos.w = 1.0;

	/*
	// DEBUG
	output.pos.xy = input.pos.xy;
	output.pos.z = lerp(1.0, 0.0, input.pos.z);
	output.pos.w = 1.0;
	// DEBUG
	*/

	return output;
}