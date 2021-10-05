// Copyright (c) 2021 Leo Reyes
// This shader is used to display animated light maps.
// Transparent areas won't change the previous contents; but this shader
// can be used to render solid areas just like the regular pixel shader.
// The main difference with the regular light map shader is that the alpha
// of the light map is not multiplied by 10.
// Licensed under the MIT license. See LICENSE.txt
#include "shader_common.h"
#include "HSV.h"
#include "shading_system.h"
#include "PixelShaderTextureCommon.h"

Texture2D    texture0 : register(t0);
SamplerState sampler0 : register(s0);

struct PixelShaderInput
{
	float4 pos    : SV_POSITION;
	float4 color  : COLOR0;
	float2 tex    : TEXCOORD0;
	float4 pos3D  : COLOR1;
	float4 normal : NORMAL;
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
	float  alpha = texelColor.w;
	float3 color = texelColor.rgb;
	float3 HSV = RGBtoHSV(color);
	const uint ExclusiveMask = special_control & SPECIAL_CONTROL_EXCLUSIVE_MASK;
	if (ExclusiveMask == SPECIAL_CONTROL_BLACK_TO_ALPHA)
		alpha = HSV.z;

	float3 diffuse = lerp(input.color.xyz, 1.0, fDisableDiffuse);
	float3 P = input.pos3D.xyz;
	float  SSAOAlpha = saturate(min(alpha - fSSAOAlphaOfs, fPosNormalAlpha));
	// Zero-out the bloom mask.
	output.bloom = 0;
	output.color = texelColor;
	output.pos3D = float4(P, SSAOAlpha);
	output.ssMask = 0;

	// hook_normals code:
	float3 N = normalize(input.normal.xyz * 2.0 - 1.0);
	N.y = -N.y; // Invert the Y axis, originally Y+ is down
	N.z = -N.z;
	output.normal = float4(N, SSAOAlpha);

	// SSAO Mask/Material, Glossiness, Spec_Intensity
	// Glossiness is multiplied by 128 to compute the exponent
	//output.ssaoMask = float4(fSSAOMaskVal, DEFAULT_GLOSSINESS, DEFAULT_SPEC_INT, alpha);
	// ssaoMask.r: Material
	// ssaoMask.g: Glossiness
	// ssaoMask.b: Specular Intensity
	output.ssaoMask = float4(fSSAOMaskVal, fGlossiness, fSpecInt, alpha);
	// SS Mask: Normal Mapping Intensity, Specular Value, Shadeless
	output.ssMask = float4(fNMIntensity, fSpecVal, fAmbient, alpha);

	// Process light textures (make them brighter in 32-bit mode)
	output.normal.a = 0;
	//output.ssaoMask.r = SHADELESS_MAT;
	output.ssMask = 0; // Normal Mapping intensity --> 0
	//output.pos3D = 0;
	//output.normal = 0;
	//output.diffuse = 0;
	// This is a light texture, process the bloom mask accordingly
	float val = HSV.z;
	// Enhance = true
	if (bIsLightTexture > 1) {
		// Make the light textures brighter in 32-bit mode
		HSV.z *= 1.25;
		//val *= 1.25; // Not sure if it's a good idea to increase val here
		// It's not! It'll make a few OPTs bloom when they didn't in 1.1.1
		// The alpha for light textures is either 0 or >0.1, so we multiply by 10 to
		// make it [0, 1]
		//alpha *= 10.0; <-- Main difference with PixelShaderTexture
		color = saturate(HSVtoRGB(HSV)); // <-- Difference wrt PixelShaderTexture
	}
	// We can't do smoothstep(0.0, 0.8, val) because the hangar will bloom all over the
	// place. Many other textures may have similar problems too.
	float bloom_alpha = smoothstep(0.75, 0.85, val) * smoothstep(0.45, 0.55, alpha);
	//const float bloom_alpha = smoothstep(0.75, 0.81, val) * smoothstep(0.45, 0.51, alpha);
	output.bloom = float4(bloom_alpha * val * color, bloom_alpha);
	output.ssaoMask.ra = bloom_alpha;
	output.ssMask.a = bloom_alpha;
	
	output.color = float4(color, alpha);
	output.bloom.rgb *= fBloomStrength;
	if (ExclusiveMask == SPECIAL_CONTROL_ALPHA_IS_BLOOM_MASK) {
		output.bloom = lerp(output.bloom, float4(0, 0, 0, 1), alpha);
		output.ssaoMask = lerp(float4(0, 0, 0, 1), float4(output.ssaoMask.rgb, alpha), alpha);
		output.ssMask = lerp(float4(0, 0, 0, 1), float4(output.ssMask.rgb, alpha), alpha);
	}
	//if (ExclusiveMask == SPECIAL_CONTROL_BLOOM_MULT_ALPHA)
	//	output.color = float4(alpha, alpha, alpha, 1.0);
	if (bInHyperspace && output.bloom.a < 0.5) {
		//output.color.a = 1.0;
		discard; // VERIFIED: Works fine in Win7
	}
	return output;
}