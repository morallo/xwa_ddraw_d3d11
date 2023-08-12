// Copyright (c) 2014 Jérémy Ansel
// Licensed under the MIT license. See LICENSE.txt
// Extended for VR by Leo Reyes, 2019
#include "VertexShaderMatrixCB.h"

// MainShadersCBStruct
cbuffer ConstantBuffer : register(b3)
{
	float scale;
	float aspect_ratio;
	float parallax;
	float brightness; // This isn't used in the vertex shader

	float use_3D; // Use the 3D projection matrix or not
};

struct VertexShaderInput
{
	float2 pos    : POSITION;
	float2 tex    : TEXCOORD;
#ifdef INSTANCED_RENDERING
	uint   instId : SV_InstanceID;
#endif
};

struct PixelShaderInput
{
	float4 pos    : SV_POSITION;
	float2 tex    : TEXCOORD;
#ifdef INSTANCED_RENDERING
	uint   viewId : SV_RenderTargetArrayIndex;
#endif
};

/* This shader is used to render the concourse, menu and other 2D elements. */
PixelShaderInput main(VertexShaderInput input)
{
	PixelShaderInput output;

#ifdef INSTANCED_RENDERING
	// Use the following in SteamVR and DirectSBS modes
	if (use_3D > 0.5) {
		// Upgrade to 3D:
		float4 P = float4(
			scale * aspect_ratio * input.pos.x,
			scale * input.pos.y,
			-(30.0 + parallax), // The Concourse is placed 30m away: enough space for the Tech Library
			1);
		// Apply the current view matrix
		P = mul(fullViewMatrix, P);
		// Project to 2D
		output.pos = mul(projEyeMatrix[input.instId], P);
	}
	else
	{
		// Use this for the original 2D version of the game:
		output.pos = float4((input.pos.x) * scale * aspect_ratio, input.pos.y * scale, parallax, 1.0f);
	}
#else
	// Use this for the original 2D version of the game:
	output.pos = float4((input.pos.x) * scale * aspect_ratio, input.pos.y * scale, parallax, 1.0f);
#endif

	output.tex = input.tex;
#ifdef INSTANCED_RENDERING
	// Pass forward the instance ID to choose the right RTV for each eye
	output.viewId = input.instId;
#endif
	return output;
}

/*
PixelShaderInput main_old(VertexShaderInput input)
{
	PixelShaderInput output;

	// This shader is used to render the concourse, menu and other 2D elements.
	output.pos = float4((input.pos.x + parallax) * scale * aspect_ratio, input.pos.y * scale, 0.5f, 1.0f);
	//output.pos = float4(input.pos.x, input.pos.y, 0.5f, 1.0f);
	output.tex = input.tex;

	return output;
}
*/