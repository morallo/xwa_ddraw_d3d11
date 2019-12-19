// Copyright Leo Reyes, 2019.
// Licensed under the MIT license. See LICENSE.txt
// This shader draws the cockpit on top of the blurred background during
// post-hyper-exit
#include "HSV.h"

// The foreground texture (shadertoyBuf)
Texture2D    fgTex     : register(t0);
SamplerState fgSampler : register(s0);

// The background texture (shadertoyAuxBuf)
Texture2D    bgTex     : register(t1);
SamplerState bgSampler : register(s1);

// The hyperspace effect texture (offscreenAsInput)
Texture2D    effectTex     : register(t2);
SamplerState effectSampler : register(s2);

struct PixelShaderInput
{
	float4 pos : SV_POSITION;
	float2 uv  : TEXCOORD;
};

/*
struct PixelShaderInput
{
	float4 pos    : SV_POSITION;
	float4 color  : COLOR0;
	float2 uv     : TEXCOORD0;
	float4 pos3D  : COLOR1;
};
*/

struct PixelShaderOutput
{
	float4 color    : SV_TARGET0;
	float4 bloom	    : SV_TARGET1;
};

static const float3 bloom_col = float3(0.5, 0.5, 1);
#define bloom_strength 2.0

PixelShaderOutput main(PixelShaderInput input)
{
	PixelShaderOutput output;
	output.bloom = 0.0;
	float bloom = 0.0;

	float4 fgColor = fgTex.Sample(fgSampler, input.uv);
	float4 bgColor = bgTex.Sample(bgSampler, input.uv);
	float4 effectColor = effectTex.Sample(effectSampler, input.uv);

	float fg_alpha = fgColor.a;
	float lightness = dot(0.333, effectColor.rgb);
	// Combine the background with the hyperspace effect
	float3 color = lerp(bgColor.rgb, effectColor.rgb, lightness);
	// Combine the previous textures with the foreground (cockpit)
	color = lerp(color, fgColor.rgb, fg_alpha);
	output.color = float4(color, 1.0);
	// Fix the bloom
	bloom = lightness;
	output.bloom = float4(bloom_strength * bloom_col * bloom, bloom);
	output.bloom *= 1.0 - fg_alpha; // Hide the bloom mask wherever the foreground is solid
	return output;
}
