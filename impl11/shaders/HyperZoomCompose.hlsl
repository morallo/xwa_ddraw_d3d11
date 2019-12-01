// Copyright Leo Reyes, 2019.
// Licensed under the MIT license. See LICENSE.txt
#include "HSV.h"

// The foreground texture (shadertoyBuf)
Texture2D    texture0 : register(t0);
SamplerState sampler0 : register(s0);

// The background texture (shadertoyAuxBuf)
Texture2D    texture1 : register(t1);
SamplerState sampler1 : register(s1);

static float METRIC_SCALE_FACTOR = 25.0;

/*
struct PixelShaderInput
{
	float4 pos    : SV_POSITION;
	float4 color  : COLOR0;
	float2 tex    : TEXCOORD0;
	float4 pos3D  : COLOR1;
};
*/

struct PixelShaderInput
{
	float4 pos : SV_POSITION;
	float2 uv  : TEXCOORD;
};

struct PixelShaderOutput
{
	float4 color    : SV_TARGET0;
	//float4 bloom    : SV_TARGET1;
	//float4 pos3D    : SV_TARGET2;
	//float4 normal   : SV_TARGET3;
	//float4 ssaoMask : SV_TARGET4;
};

/*
cbuffer ConstantBuffer : register(b0)
{
	float brightness;			// Used to dim some elements to prevent the Bloom effect -- mostly for ReShade compatibility
	uint DynCockpitSlots;		// (Unused here) How many DC slots will be used.
	uint bUseCoverTexture;		// (Unused here) When set, use the first texture as cover texture for the dynamic cockpit
	uint bIsHyperspaceAnim;		// 1 if we're rendering the hyperspace animation
	// 16 bytes

	uint bIsLaser;				// 1 for Laser objects, setting this to 2 will make them brighter (intended for 32-bit mode)
	uint bIsLightTexture;		// 1 if this is a light texture, 2 will make it brighter (intended for 32-bit mode)
	uint bIsEngineGlow;			// 1 if this is an engine glow textures, 2 will make it brighter (intended for 32-bit mode)
	uint bIsHyperspaceStreak;	// 1 if we're rendering hyperspace streaks
	// 32 bytes

	float fBloomStrength;	// General multiplier for the bloom effect
	float fPosNormalAlpha;	// Override for pos3D and normal output alpha
	float fSSAOMaskVal;		// SSAO mask value
	float fSSAOAlphaOfs;		// Additional offset substracted from alpha when rendering SSAO. Helps prevent halos around transparent objects.
	// 48 bytes
};
*/

PixelShaderOutput main(PixelShaderInput input)
{
	PixelShaderOutput output;
	float4 fgColor = texture0.Sample(sampler0, input.uv);
	float4 bgColor = texture1.Sample(sampler1, input.uv);
	
	float lightness = dot(0.333, fgColor.rgb);
	output.color.rgb = lerp(bgColor.rgb, fgColor.rgb, fgColor.w);
	//output.color.rgb = lerp(bgColor.rgb, fgColor.rgb, lightness);
	output.color.a = 1.0;
	return output;
}
