// Copyright (c) 2025 Leo Reyes
// Licensed under the MIT license. See LICENSE.txt
// This shader should only be called to render the HUD FG/BG
#include "shader_common.h"
#include "PixelShaderTextureCommon.h"

// texture0 == Depth Buffer
Texture2D    texture0 : register(t0);
SamplerState sampler0 : register(s0);

// texture1 == Enhanced HUD contents
Texture2D    texture1 : register(t1);

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

	const float4 pos3D = texture0.Sample(sampler0, input.tex);
	// The cockpit tends to be less than 50m in depth in fron to us, so this is probably
	// a good threshold to reject any display that is closer than that:
	if (pos3D.z < 50.0f)
		discard;

	const float4 texelText = texture1.Sample(sampler0, input.tex);
	// Compute the text alpha from approx Luma
	const float textAlpha = saturate(10.0 * dot(float3(0.33, 0.5, 0.16), texelText.rgb));
	
	output.color = float4(texelText.rgb, textAlpha);
	return output;
}
