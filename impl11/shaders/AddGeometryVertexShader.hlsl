// Copyright (c) 2020 Leo Reyes
// Licensed under the MIT license. See LICENSE.txt
// This shader is used to render the speed effect particles
#include "VertexShaderCBuffer.h"
#include "VertexShaderMatrixCB.h"
#include "ShadertoyCBuffer.h"
#include "shadow_mapping_common.h"
#include "metric_common.h"

struct VertexShaderInput
{
	float4 pos		: POSITION;
	float4 color		: COLOR0;
	float4 normal   : COLOR1;
	float2 tex		: TEXCOORD;
	uint instId : SV_InstanceID;
};

struct PixelShaderInput
{
	float4 pos		: SV_POSITION;
	float4 color		: COLOR0;
	float4 normal	: COLOR1;
	float2 tex		: TEXCOORD;
	uint viewId : SV_RenderTargetArrayIndex;
};

PixelShaderInput main(VertexShaderInput input)
{
	PixelShaderInput output;

	// input.pos is OBJ * 1.64 * (+/-1 on each axis, depends on the MAT file)
	// OBJ-3D to cockpit camera view:
	float4 P = mul(Camera, float4(input.pos.xyz, 1.0));
	
	// The 2D point is now in DirectX coords (-1..1) that are compatible with XWA 2D (minus scale)
	if (bFullTransform < 0.5)
	{
		// Regular Vertex Shader
		// Original transform chain, including 2D projection:
		//P.x /= sm_aspect_ratio;
		//P.x = FOVscale * (P.x / P.z);
		//P.y = FOVscale * (P.y / P.z) + y_center;

		// Adjust the point for differences in FOV and y_center AND PROJECT TO 2D:
		P.xy = mr_FOVscale / P.z * float2(P.x / mr_aspect_ratio, P.y + P.z * mr_y_center / mr_FOVscale);

		// Fix the depth
		P.z = 0.0; // Depth value -- this makes the point visible
		P.w = 1.0;
		//P.w = P.z / FOVscale;
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
		// Remove the scale (1.64) we added when loading the OBJ since we need fully-metric
		// coords for the VR path:
		P /= mr_shadow_OBJ_scale;

		// Project:
		P.z = -P.z;
		output.pos = mul(projEyeMatrix[0], float4(P.xyz, 1.0));
		
		/*
		VR PATH

		The reason we're projecting to 2D, back-projecting to 3D and then projecting
		to 2D again is because the first 3D -> 2D projection matches OBJ coords to XWA 2D
		coords. Then these coords can be back-projected into VR 3D so that they can be
		projected again into VR 2D.
		I seriously need to do some refactoring here.
		*/

		/*
		// This code does a correct back-project -> project; but it requires the adjustment for
		// differences in FOV and y_center performed in the regular path and it's missing the
		// division by 1.64. Since the simple projection is better, this code is no longer needed
		//temp.z = sm_FOVscale * (1.0f / rhw) * g_fOBJ_Z_MetricMult;
		//rhw = 1.0 / (temp.z / g_fOBJ_Z_MetricMult / sm_FOVscale)
		// temp.z is now metric 3D minus the g_fOBJCurMetricScale scale factor
		float tempz = P.z / mr_cur_metric_scale; // Should be equal to: sm_FOVscale * (1.0f / rhw) * g_fOBJ_Z_MetricMult;
		float FOVscaleZ = mr_FOVscale / tempz;

		// temp.xy is already in DirectX coords [-1..1]
		P.x = P.x / FOVscaleZ * mr_aspect_ratio;
		P.y = P.y / FOVscaleZ - tempz * mr_y_center / mr_FOVscale;

		// g_fOBJCurMetricScale depends on the current in-game height and is computed
		// whenever a new FOV is applied. See ComputeHyperFOVParams()
		P.xy *= mr_cur_metric_scale;
		// P.z is already "pre-multiplied" by mr_cur_metric_scale

		float3 Q = P.xyz;
		// Project again
		Q.z = -Q.z;
		Q = mul(eyeMatrix, float4(Q, 1)).xyz;
		output.pos = mul(projEyeMatrix, float4(Q, 1.0));
		*/

		// DirectX will divide output.pos by output.pos.w, so we don't need to do it here,
		// plus we probably can't do it here in the case of SteamVR anyway...

		// The following value is like the depth-buffer value. Setting it to 2 will remove
		// this point
		output.pos.z = 0.0f; // It's OK to ovewrite z after the projection, I'm doing this in SBSVertexShader already
	}

	output.color  = input.color.zyxw;
	output.tex    = input.tex;
	output.normal = input.normal;
	// Pass forward the instance ID to choose the right RTV for each eye
	output.viewId = input.instId;
	return output;
}
