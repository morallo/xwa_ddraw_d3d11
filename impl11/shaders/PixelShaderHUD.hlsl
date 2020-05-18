// Copyright (c) 2019 Leo Reyes
// Licensed under the MIT license. See LICENSE.txt
// This shader should only be called to render the HUD FG/BG
#include "shader_common.h"
#include "PixelShaderTextureCommon.h"

// texture0 == HUD foreground
Texture2D    texture0 : register(t0);
SamplerState sampler0 : register(s0);

// texture1 == HUD background
Texture2D    texture1 : register(t1);
SamplerState sampler1 : register(s1);

// texture2 == HUD Text
Texture2D    texture2 : register(t2);
SamplerState sampler2 : register(s2);

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

// DCPixelShaderCBuffer, _PSConstantBufferDC, g_DCPSCBuffer
cbuffer ConstantBuffer : register(b1)
{
	float4 src[MAX_DC_COORDS_PER_TEXTURE];		   // HLSL packs each element in an array in its own 4-vector (16 bytes) slot, so .xy is src0 and .zw is src1
	float4 dst[MAX_DC_COORDS_PER_TEXTURE];
	uint4  bgColor[MAX_DC_COORDS_PER_TEXTURE / 4]; // Background colors to use for the dynamic cockpit, this divide by 4 is because HLSL packs each elem in a 4-vector,
												   // So each elem here is actually 4 bgColors.

	float ct_brightness;				   // Cover texture brightness. In 32-bit mode the cover textures have to be dimmed.
	float unused1, unused2, unused3;
};

PixelShaderOutput main(PixelShaderInput input)
{
	PixelShaderOutput output;
	float4 texelColor   = texture0.Sample(sampler0, input.tex);
	float4 texelColorBG = texture1.Sample(sampler1, input.tex);
	float4 texelText    = texture2.Sample(sampler2, input.tex);
	float  alpha   = texelColor.w;
	float  alphaBG = texelColorBG.w;
	float3 diffuse = input.color.xyz;
	uint i;

	// Fix the text alpha and blend it with the HUD foreground
	float textAlpha = saturate(3.25 * dot(0.333, texelText.rgb));
	texelColor.rgb = lerp(texelColor.rgb, texelText.rgb, textAlpha);
	texelColor.w = max(texelColor.w, textAlpha);
	alpha = texelColor.w;
	
	output.color  = texelColor;

	// Render the captured HUD, execute the move_region commands.
	// texture0 == HUD foreground
	// texture1 == HUD background
	
	// When DC is off (DynCockpitSlots == 0) we can return early by
	// simply combining texelColor with texelColorBG
	if (DynCockpitSlots == 0) {
		// Do the alpha blending
		texelColor.xyz = lerp(texelColorBG.xyz, texelColor.xyz, alpha);
		texelColor.w += 3.25 * alphaBG;
		output.color = texelColor;
		return output;
	}

	// Execute the move_region commands: erase source regions
	[unroll]
	for (i = 0; i < DynCockpitSlots; i++)
		if (all(input.tex.xy >= src[i].xy) && all(input.tex.xy <= src[i].zw))
		{
			texelColor.w = 0;
			alpha = 0;
			alphaBG = 0;

			// DEBUG: Highlight the source regions that will be moved in RED
			/*
			texelColor.xyz = float3(1.0, 0.0, 0.0);
			texelColor.w = 1.0;
			alpha = 1;
			alphaBG = 0;
			*/
			// DEBUG
		}

	// Execute the move_region commands: copy regions
	[unroll]
	for (i = 0; i < DynCockpitSlots; i++) 
	{
		float2 delta = dst[i].zw - dst[i].xy;
		float2 s = (input.tex - dst[i].xy) / delta;
		float2 dyn_uv = lerp(src[i].xy, src[i].zw, s);
		if (all(dyn_uv >= src[i].xy) && all(dyn_uv <= src[i].zw))
		{
			// Sample the HUD FG and BG from a different location:
			texelColor   = texture0.Sample(sampler0, dyn_uv);
			texelColorBG = texture1.Sample(sampler1, dyn_uv);
			texelText    = texture2.Sample(sampler2, dyn_uv);
			// Fix the text alpha and blend it with the HUD foreground
			float textAlpha = saturate(3.25 * dot(0.333, texelText.rgb));
			texelColor.rgb  = lerp(texelColor.rgb, texelText.rgb, textAlpha);
			texelColor.w    = max(texelColor.w, textAlpha);

			alpha   = texelColor.w;
			alphaBG = texelColorBG.w;
		}
	}

	// Do the alpha blending
	texelColor.xyz = lerp(texelColorBG.xyz, texelColor.xyz, alpha);
	texelColor.w += 3.25 * alphaBG;
	output.color = texelColor;
	return output;
}
