// Copyright (c) 2020, 2021 Leo Reyes
// Licensed under the MIT license. See LICENSE.txt
// Simplified version of PixelShaderTexture. This shader is used to render
// textures with alpha that we don't want to render as glass.
// if fBloomStrength is not zero, then bloom will be applied and modulated by the alpha
// of the texture.
// Light Textures are not handled in this shader. This shader should not be used with
// illumination textures.
#include "XwaD3dCommon.hlsl"
#include "shader_common.h"
#include "HSV.h"
#include "shading_system.h"
#include "PixelShaderTextureCommon.h"

Texture2D texture0 : register(t0); // This is the regular color texture
Texture2D texture1 : register(t1); // If present, this is the light texture
SamplerState sampler0 : register(s0);

// pos3D/Depth buffer has the following coords:
// X+: Right
// Y+: Up
// Z+: Away from the camera
// (0,0,0) is the camera center, (0,0,Z) is the center of the screen

struct PixelShaderInput
{
	float4 pos    : SV_POSITION;
	float4 pos3D  : COLOR1;
	float4 normal : NORMAL;
	float2 tex	  : TEXCOORD;
	//float4 color  : COLOR0;
};

struct PixelShaderOutput
{
	float4 color    : SV_TARGET0;
	float4 bloom    : SV_TARGET1;
	float4 pos3D    : SV_TARGET2;
	float4 normal   : SV_TARGET3;
	float4 ssaoMask : SV_TARGET4;
	float4 ssMask   : SV_TARGET5;
};

// TODO: This shader probably doesn't need to pay attention to special_control, or use AuxColor anymore.
PixelShaderOutput main(PixelShaderInput input)
{
	PixelShaderOutput output;
	float2 UV = input.tex * float2(AspectRatio, 1) + Offset.xy;
	if (Clamp) UV = saturate(UV);
	float4 texelColor = AuxColor * texture0.Sample(sampler0, UV);
	//if (special_control & SPECIAL_CONTROL_BLAST_MARK) texelColor = texture0.Sample(sampler0, (input.tex * 0.5) + 0.25);
	if (special_control & SPECIAL_CONTROL_BLAST_MARK) texelColor = texture0.Sample(sampler0, (input.tex * 0.35) + 0.3);
	//if (special_control & SPECIAL_CONTROL_BLAST_MARK) texelColor = texture0.Sample(sampler0, (input.tex * 0.25) + 0.35);
	float  alpha = texelColor.a;
	float3 HSV = RGBtoHSV(texelColor.rgb);
	uint ExclusiveMask = special_control & SPECIAL_CONTROL_EXCLUSIVE_MASK;
	
	if (ExclusiveMask == SPECIAL_CONTROL_BLACK_TO_ALPHA)
		alpha = HSV.z;

	float3 P = input.pos3D.xyz;
	float  SSAOAlpha = saturate(min(alpha - fSSAOAlphaOfs, fPosNormalAlpha));
	// Zero-out the bloom mask.
	output.bloom = float4(0, 0, 0, alpha);
	output.color = float4(brightness * texelColor.xyz, alpha);
	output.pos3D = float4(P, SSAOAlpha);
	output.ssMask = 0;
	
	float3 N = normalize(input.normal.xyz);
	N.yz = -N.yz; // Invert the Y axis, originally Y+ is down
	output.normal = float4(N, SSAOAlpha);

	// ssaoMask: Material, Glossiness, Specular Intensity
	output.ssaoMask = float4(fSSAOMaskVal, fGlossiness, fSpecInt, alpha);
	// SS Mask: Glass, Specular Value, Shadeless
	output.ssMask = float4(0, fSpecVal, max(fAmbient, bIsShadeless), alpha);

	if (renderTypeIllum == 1)
	{
		// We have a lightmap texture
		const float4 texelColorIllum = texture1.Sample(sampler0, input.tex);
		// The alpha for light textures is either 0 or >0.1, so we multiply by 10 to make it [0, 1]
		const float alphaLight = texelColorIllum.a * 5.0;
		float3 illumColor = texelColorIllum.rgb;
		// This is a light texture, process the bloom mask accordingly
		float3 HSV = RGBtoHSV(illumColor);
		float val = HSV.z;
		// Make the light textures brighter in 32-bit mode
		HSV.z *= 1.25;

		illumColor = HSVtoRGB(HSV);

		const float valFactor = bIsTransparent ? 1.0 : smoothstep(0.75, 0.85, val);
		// The first term below uses "val" (lightness) to apply bloom on light areas. That's how we avoid
		// applying bloom on dark areas.
		//const float bloom_alpha = valFactor * smoothstep(0.45, 0.55, alphaLight);
		const float bloom_alpha = valFactor * smoothstep(0.2, 0.90, alphaLight);
		// Apply the bloom strength to this lightmap
		output.bloom = float4(fBloomStrength * bloom_alpha * val * illumColor, bloom_alpha);
		// Write an emissive material where there's bloom:
		output.ssaoMask.r = lerp(output.ssaoMask.r, 0, bloom_alpha);
		//output.ssMask.ba  = bloom_alpha; // Shadeless area
		output.ssMask.b = lerp(output.ssMask.b, 1, bloom_alpha); // Shadeless area
		// Replace the current color with the lightmap color, where appropriate:
		output.color.rgb = lerp(output.color.rgb, illumColor, bloom_alpha);
		if (bInHyperspace && output.bloom.a < 0.5)
			discard;
		return output;
	}

	// Compute bloom from value:
	// The old code used the color's value to compute bloom, but now we're actually using
	// the lightmap. Some differences may happen because of this change.
	/*
	if (HSV.z >= 0.8)
	{
		float bloom_alpha = saturate(fBloomStrength);
		output.bloom = float4(fBloomStrength * texelColor.rgb, alpha);
		output.ssMask.b = bloom_alpha;
		//output.ssaoMask.ga = 1; // Maximum glossiness on light areas
		output.ssaoMask.a = bloom_alpha;
		//output.ssaoMask.b = 0.15; // Low spec intensity
		output.ssaoMask.b = 0.15 * bloom_alpha; // Low spec intensity
	}
	*/

	return output;
}
