// Copyright Leo Reyes, 2019.
// Licensed under the MIT license. See LICENSE.txt
// This shader draws the cockpit on top of the blurred background during
// post-hyper-exit
#include "HSV.h"
#include "ShaderToyDefs.h"

// ShadertoyCBuffer
cbuffer ConstantBuffer : register(b7)
{
	float iTime, twirl, bloom_strength, unused;
	// 16 bytes
	float2 iResolution;
	uint bDirectSBS;
	float y_center;
	// 32 bytes
	float2 p0, p1; // Limits in uv-coords of the viewport
	// 48 bytes
	matrix viewMat;
	// 112 bytes
	uint bDisneyStyle, hyperspace_phase;
	float tunnel_speed, FOVscale;
	// 128 bytes
};

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

// The HyperZoom effect probably can't be applied in this shader because it works
// on the full screen. Instead the HyperZoom must be applied SBS or using independent
// buffers

static const float3 bloom_col = float3(0.5, 0.5, 1);
//#define bloom_strength 2.0

PixelShaderOutput main(PixelShaderInput input)
{
	PixelShaderOutput output;
	output.bloom = 0.0;
	float bloom = 0.0;

	float4 fgColor = fgTex.Sample(fgSampler, input.uv);
	float4 bgColor = bgTex.Sample(bgSampler, input.uv);
	float4 effectColor = effectTex.Sample(effectSampler, input.uv);

	/*
	// HyperZoom
	// Apply the zoom effect
	if (bBGTextureAvailable) {
		vec2 fragCoord = input.uv * iResolution.xy;
		// Convert pixel coord into uv centered at the origin
		float2 uv = fragCoord / iResolution.xy - scr_center;
		bgColor.rgb = HyperZoom(uv);
		bgColor.a = 1.0;
	}
	// HyperZoom
	*/

	float3 color;
	float fg_alpha = fgColor.a;
	float lightness = dot(0.333, effectColor.rgb);
	// Combine the background with the hyperspace effect
	if (hyperspace_phase == 2) {
		// Replace the background with the tunnel
		color = effectColor.rgb;
	}
	else {
		// Blend the trails with the background
		color = lerp(bgColor.rgb, effectColor.rgb, lightness);
	}
	// Combine the previous textures with the foreground (cockpit)
	color = lerp(color, fgColor.rgb, fg_alpha);
	output.color = float4(color, 1.0);

	// Fix the bloom
	if (hyperspace_phase == 1 || hyperspace_phase == 3 || hyperspace_phase == 4) {
		bloom = lightness;
	}
	else {
		bloom = 0.65 * smoothstep(0.55, 1.0, lightness);
	}
	output.bloom = float4(bloom_strength * bloom_col * bloom, bloom);
	output.bloom *= 1.0 - fg_alpha; // Hide the bloom mask wherever the foreground is solid
	return output;
}
