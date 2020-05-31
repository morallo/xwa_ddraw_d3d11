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

// Use this shader for 3D Shadow OBJ
SHADOW_PS_INPUT main(VertexShaderInput input)
{
	SHADOW_PS_INPUT output;

	// The input is expected to be OBJ-3D

	// OBJ-3D to camera view:
	float4 P = mul(Camera, float4(input.pos.xyz, 1.0));

	// Project the OBJ point to 2D:
	//P.x /= sm_aspect_ratio;
	//P.x = sm_FOVscale * (P.x / P.z);
	//P.y = sm_FOVscale * (P.y / P.z) + sm_y_center;

	P.xy = sm_FOVscale * float2(P.x / sm_aspect_ratio, P.y + P.z * sm_y_center / sm_FOVscale);
	// Project the point. The P.z here is OBJ-3D plus Camera transform
	P.xy /= P.z;

	// The point is now in DirectX 2D coord sys (-1..1). The depth of the point is in P.z
	// The OBJ-2D should match XWA 2D at this point. Let's back-project so that
	// they're in the same coord sys
	P.xy *= vpScale.z * float2(sm_aspect_ratio, 1);
	//P.z = METRIC_SCALE_FACTOR * w; // This value was determined empirically
	P.z *= sm_z_factor;
	P = float4(P.z * P.xy / DEFAULT_FOCAL_DIST, P.z, 1.0);

	// The point is now in XWA 3D, with the POV at the origin.
	// let's apply the light transform and project it from the
	// light's point of view.
	P = mul(lightWorldMatrix[light_index], P);

	// xy: Parallel projection
	// Map the useful Z depth into the range 0..0.98:
	//output.pos = float4(P.xy, (P.z - OBJminZ) / OBJrange * 0.98, 1.0);
	output.pos = float4(P.xy, MetricZToDepth(light_index, P.z), 1.0);
	return output;
}