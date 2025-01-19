// Copyright (c) 2025 Leo Reyes
// Licensed under the MIT license. See LICENSE.txt
// This shader should only be called to render the HUD FG/BG
#include "shader_common.h"
#include "PixelShaderTextureCommon.h"

// texture0 == Depth Buffer
Texture2D    depthBuf : register(t0);
SamplerState sampler0 : register(s0);

// texture1 == Enhanced HUD contents
Texture2D enhancedHudBuf : register(t1);

struct PixelShaderInput
{
	float4 pos   : SV_POSITION;
	float4 color : COLOR0;
	float2 tex   : TEXCOORD;
};

struct PixelShaderOutput
{
	float4 color : SV_TARGET0;
};

PixelShaderOutput main(PixelShaderInput input)
{
	PixelShaderOutput output;

	const float4 pos3D = depthBuf.Sample(sampler0, input.tex);
	// The cockpit tends to be less than 25m in depth in front of us, so this is probably
	// a good threshold to reject any display that is closer than that:
	if (pos3D.z < 25.0f)
		discard;

	output.color = enhancedHudBuf.Sample(sampler0, input.tex);
	return output;
}
