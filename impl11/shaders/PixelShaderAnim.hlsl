// Copyright (c) 2021 Leo Reyes
// This shader is used to display both animated textures & light maps.
// Transparent areas won't change the previous contents; but this shader
// can be used to render solid areas just like the regular pixel shader.
// The main difference with the regular light map shader is that the alpha
// of the light map is not multiplied by 10.
// This shader also has code from the old version of PixelShaderNoGlass.
// Licensed under the MIT license. See LICENSE.txt
#include "XwaD3dCommon.hlsl"
#include "shader_common.h"
#include "HSV.h"
#include "shading_system.h"
#include "PixelShaderTextureCommon.h"

Texture2D    texture0 : register(t0);
Texture2D	 texture1 : register(t1); // If present, this is the light texture
SamplerState sampler0 : register(s0);

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

PixelShaderOutput main(PixelShaderInput input)
{
	PixelShaderOutput output;
	float2 UV = input.tex * float2(AspectRatio, 1) + Offset.xy;
	if (Clamp) UV = saturate(UV);

	float4 texelColor = AuxColor * texture0.Sample(sampler0, UV);
	if (special_control & SPECIAL_CONTROL_BLAST_MARK) texelColor = texture0.Sample(sampler0, (input.tex * 0.35) + 0.3);
	float  alpha = texelColor.a;
	float3 HSV = RGBtoHSV(texelColor.rgb);
	uint ExclusiveMask = special_control & SPECIAL_CONTROL_EXCLUSIVE_MASK;
	if (ExclusiveMask == SPECIAL_CONTROL_BLACK_TO_ALPHA)
		alpha = HSV.z;

	float4 texelColorLight = AuxColorLight * texture1.Sample(sampler0, UV);
	float  alphaLight = texelColorLight.a;
	float3 colorLight = texelColorLight.rgb;
	float3 HSVLight = RGBtoHSV(colorLight);
	const uint ExclusiveMaskLight = special_control_light & SPECIAL_CONTROL_EXCLUSIVE_MASK;
	if (ExclusiveMaskLight == SPECIAL_CONTROL_BLACK_TO_ALPHA)
		alphaLight = HSVLight.z;

	float3 P = input.pos3D.xyz;
	float  SSAOAlpha = saturate(min(alpha - fSSAOAlphaOfs, fPosNormalAlpha));
	// Zero-out the bloom mask and provide default output values
	output.bloom = 0;
	output.color = texelColor;
	output.pos3D = float4(P, SSAOAlpha);
	output.ssMask = 0;

	float3 N = normalize(input.normal.xyz);
	N.y = -N.y; // Invert the Y axis, originally Y+ is down
	N.z = -N.z;
	output.normal = float4(N, SSAOAlpha);

	// ssaoMask: Material, Glossiness, Specular Intensity
	output.ssaoMask = float4(fSSAOMaskVal, fGlossiness, fSpecInt, alpha);
	// SS Mask: unused, Specular Value, Shadeless
	output.ssMask = float4(0, fSpecVal, fAmbient, alpha);

	// The regular layer might have transparency. If we're in hyperspace, we don't want to show
	// the background through it, so we mix it with a black color
	// Update: looks like we don't need to do this anymore (?) In fact, enabling this line will
	// cause the cockpit glass to go black if it's damaged before jumping into hyperspace
	//if (bInHyperspace) output.color = float4(lerp(float3(0,0,0), output.color.rgb, alpha), 1);

	if (renderTypeIllum == 1)
	{
		// This is a light texture, process the bloom mask accordingly
		float val = HSVLight.z;
		HSVLight.z *= 1.25;
		//alpha *= 10.0; <-- Main difference with XwaD3dPixelShader
		colorLight = saturate(HSVtoRGB(HSVLight)); // <-- Difference wrt XwaD3dPixelShader
		
		float bloom_alpha = smoothstep(0.75, 0.85, val) * smoothstep(0.45, 0.55, alphaLight);
		output.bloom = float4(bloom_alpha * val * colorLight, bloom_alpha);
		// Write an emissive material where there's bloom:
		output.ssaoMask.r = lerp(output.ssaoMask.r, bloom_alpha, bloom_alpha);
		// Set fNMIntensity to 0 where we have bloom:
		output.ssMask.r = lerp(output.ssMask.r, 0, bloom_alpha);
		// Replace the current color with the lightmap color, where appropriate:
		output.color.rgb = lerp(output.color.rgb, colorLight, alphaLight);
		//output.color.a = max(output.color.a, alphaLight);
		// Apply the bloom strength to this lightmap
		output.bloom.rgb *= fBloomStrength;
		if (ExclusiveMaskLight == SPECIAL_CONTROL_ALPHA_IS_BLOOM_MASK) {
			output.bloom = lerp(output.bloom, float4(0, 0, 0, 1), alphaLight);
			output.ssaoMask = lerp(float4(0, 0, 0, 1), float4(output.ssaoMask.rgb, alphaLight), alphaLight);
			output.ssMask = lerp(float4(0, 0, 0, 1), float4(output.ssMask.rgb, alphaLight), alphaLight);
		}
		// The following line needs to be updated, it was originally
		// intended to remove the lightmap layer, but keep the regular
		// layer. Doing discard now will remove both layers!
		//if (bInHyperspace && output.bloom.a < 0.5)
		//	discard;
	}

	return output;
}