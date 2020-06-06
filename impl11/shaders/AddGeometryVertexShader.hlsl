// Copyright (c) 2020 Leo Reyes
// Licensed under the MIT license. See LICENSE.txt
// This shader is used to render the speed effect particles
#include "ShadertoyCBuffer.h"
#include "shadow_mapping_common.h"

// VertexShaderCBuffer
cbuffer ConstantBuffer : register(b0)
{
	float4 vpScale;
	float aspect_ratio, cockpit_threshold, z_override, sz_override;
	float mult_z_override, bPreventTransform, bFullTransform, metric_mult;
};

// VertexShaderMatrixCB
cbuffer ConstantBuffer : register(b1)
{
	matrix projEyeMatrix;
	matrix viewMatrix;
	matrix fullViewMatrix;
};

struct VertexShaderInput
{
	float4 pos		: POSITION;
	float4 color	: COLOR0;
	float4 normal   : COLOR1;
	float2 tex		: TEXCOORD;
};

struct PixelShaderInput
{
	float4 pos		: SV_POSITION;
	float4 color	: COLOR0;
	float4 normal	: COLOR1;
	float2 tex		: TEXCOORD;
};

PixelShaderInput main(VertexShaderInput input)
{
	PixelShaderInput output;

	// OBJ-3D to cockpit camera view:
	float4 P = mul(Camera, float4(input.pos.xyz, 1.0));
	
	// Original transform chain, including 2D projection:
	//P.x /= sm_aspect_ratio;
	//P.x = FOVscale * (P.x / P.z);
	//P.y = FOVscale * (P.y / P.z) + y_center;

	// Adjust the point for differences in FOV and y_center AND PROJECT TO 2D:
	P.xy = FOVscale / P.z * float2(P.x / sm_aspect_ratio, P.y + P.z * y_center / FOVscale);
	
	// The 2D point is now in DirectX coords (-1..1) that are compatible with XWA 2D (minus scale)

	if (bFullTransform < 0.5)
	{
		// Regular Vertex Shader
		
		// Fix the depth
		P.z = 0.0; // Depth value -- this makes the point visible
		P.w = 1.0;
		output.pos = P;
		output.color = float4(1, 1, 0, 1);

		/*
		What does this shader tell me? I'm loading coordinates from an OBJ created in Blender.
		If we multiply lightWorldMatrix by the OBJ 3D coord and then we project it into 2D,
		the points match and transform properly with XWA's 2D coords.

		So, 3D-OBJ -> 2D XWA -> backproject -> backprojected 3D compatible with backprojected 3D from the (SBS)VertexShader
		*/
	}
	else
	{
		// VR PATH
		/*
		The reason we're projecting to 2D, back-projecting to 3D and then projecting
		to 2D again is because the first 3D->2D projection matches OBJ coords to XWA 2D
		coords. Then these coords can be back-projected into VR 3D so that they can be
		projected again into VR 2D.
		I seriously need to do some refactoring here.
		*/

		// Back-project into 3D space again
		float3 temp = P.xyz;
		// The additional 2.0 factor below is because in VR vpScale.xy is 1/{width,height}, but in non-VR it's 2/{width,height}
		// so we have to compensate
		// I'm not sure we need vpScale.z, but I'm leaving it here for completeness
		temp.xy *= vpScale.w * vpScale.z / 2.0 * float2(aspect_ratio, 1);
		// TODO: Verify that the addition of DEFAULT_FOCAL_DIST_VR didn't change this shader
		float3 Q = float3(temp.z * temp.xy / DEFAULT_FOCAL_DIST_VR, temp.z);
		// Project again
		Q.z = -Q.z;
		output.pos = mul(projEyeMatrix, float4(Q.xyz, 1.0));
		// DirectX will divide output.pos by output.pos.w, so we don't need to do it here,
		// plus we probably can't do it here in the case of SteamVR anyway...

		// The following value is like the depth-buffer value. Setting it to 2 will remove
		// this point
		output.pos.z = 0.0f; // It's OK to ovewrite z after the projection, I'm doing this in SBSVertexShader already
	}

	output.color  = input.color.zyxw;
	output.tex    = input.tex;
	output.normal = input.normal;
	return output;
}
