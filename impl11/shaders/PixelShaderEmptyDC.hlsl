// Copyright (c) 2019 Leo Reyes
// Licensed under the MIT license. See LICENSE.txt
// This shader should only be called for destination textures when DC is
// enabled. For regular textures or when the Dynamic Cockpit is disabled,
// use "PixelShader.hlsl" instead.
#include "HSV.h"

Texture2D    texture0 : register(t0);
SamplerState sampler0 : register(s0);

Texture2D    texture1 : register(t1);
SamplerState sampler1 : register(s1);

// When the Dynamic Cockpit is active:
// texture0 == cover texture and
// texture1 == HUD offscreen buffer

// If bRenderHUD is set:
// texture0 == HUD foreground
// texture1 == HUD background

struct PixelShaderInput
{
	float4 pos : SV_POSITION;
	float4 color : COLOR0;
	float2 tex : TEXCOORD0;
	float4 pos3D : COLOR1;
};

struct PixelShaderOutput
{
	float4 color    : SV_TARGET0;
	float4 bloom    : SV_TARGET1;
	float4 pos3D    : SV_TARGET2;
	float4 normal   : SV_TARGET3;
	float4 ssaoMask : SV_TARGET4;
};

cbuffer ConstantBuffer : register(b0)
{
	float brightness;		// Used to dim some elements to prevent the Bloom effect -- mostly for ReShade compatibility
	uint DynCockpitSlots;	// How many DC slots will be used. This setting was "bShadeless" previously
	uint bUseCoverTexture;	// When set, use the first texture as cover texture for the dynamic cockpit
	uint bIsHyperspaceAnim;
	// 16 bytes

	uint bIsLaser;				// 1 for Laser objects, setting this to 2 will make them brighter (intended for 32-bit mode)
	uint bIsLightTexture;		// 1 if this is a light texture, 2 will make it brighter (intended for 32-bit mode)
	uint bIsEngineGlow;			// 1 if this is an engine glow textures, 2 will make it brighter (intended for 32-bit mode)
	uint bIsHyperspaceStreak;

	float fBloomStrength;		// General multiplier for the bloom effect
	float fSSAOMaskVal;			// (Ignored) SSAO mask value
};

#define MAX_DC_COORDS 8
cbuffer ConstantBuffer : register(b1)
{
	float4 src[MAX_DC_COORDS];		  // HLSL packs each element in an array in its own 4-vector (16 bytes) slot, so .xy is src0 and .zw is src1
	float4 dst[MAX_DC_COORDS];
	uint4 bgColor[MAX_DC_COORDS / 4]; // Background colors to use for the dynamic cockpit, this divide by 4 is because HLSL packs each elem in a 4-vector,
									  // So each elem here is actually 4 bgColors.

	float ct_brightness;				  // Cover texture brightness. In 32-bit mode the cover textures have to be dimmed.
	// unused1, unused2, unused3
};

float4 uintColorToFloat4(uint color) {
	return float4(
		((color >> 16) & 0xFF) / 255.0,
		((color >> 8) & 0xFF) / 255.0,
		(color & 0xFF) / 255.0,
		1);
}

uint getBGColor(uint i) {
	uint idx = i / 4;
	uint sub_idx = i % 4;
	return bgColor[idx][sub_idx];
}

PixelShaderOutput main(PixelShaderInput input)
{
	PixelShaderOutput output;
	float4 texelColor = texture0.Sample(sampler0, input.tex);
	float alpha = texelColor.w;
	float3 diffuse = input.color.xyz;
	// Zero-out the bloom mask.
	output.bloom = float4(0, 0, 0, 0);
	output.color = texelColor;

	float3 P = input.pos3D.xyz;
	output.pos3D = float4(P, 1);

	float3 N = normalize(cross(ddx(P), ddy(P)));
	output.normal = float4(N, 1);

	output.ssaoMask = 0;

	// No DynCockpitSlots; but we're using a cover texture anyway. Clear the holes.
	// The code returns a color from this path
	//else if (bUseCoverTexture > 0) {
	// We assume the caller will set this pixel shader iff DynCockpitSlots == 0 && bUseCoverTexture > 0
	{
		// texelColor is the cover_texture right now
		float3 HSV = RGBtoHSV(texelColor.xyz);
		float4 hud_texelColor = float4(0, 0, 0, 1);
		float brightness = ct_brightness;
		if (HSV.z * alpha >= 0.8) {
			// The cover texture is bright enough, go shadeless and make it brighter
			diffuse = float3(1, 1, 1);
			// Increase the brightness:
			HSV = RGBtoHSV(texelColor.xyz);
			HSV.z *= 1.2;
			texelColor.xyz = HSVtoRGB(HSV);
			output.bloom = float4(fBloomStrength * texelColor.xyz, 1);
			brightness = 1.0;
			output.ssaoMask = 1;
		}
		texelColor = lerp(hud_texelColor, brightness * texelColor, alpha);
		output.color = float4(diffuse * texelColor.xyz, alpha);
		return output;
	}

	output.color = float4(brightness * diffuse * texelColor.xyz, texelColor.w);
	return output;
}
