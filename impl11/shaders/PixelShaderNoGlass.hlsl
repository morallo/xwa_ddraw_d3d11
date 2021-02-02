// Copyright (c) 2020, 2021 Leo Reyes
// Licensed under the MIT license. See LICENSE.txt
// Simplified version of PixelShaderTexture. Alpha-enabled textures won't be interpreted
// as glass materials. if fBloomStrength is not zero, then bloom will be applied and
// modulated by the alpha of the texture.
// Light Textures are not handled in this shader. This shader should not be used with
// illumination textures.
#include "shader_common.h"
#include "HSV.h"
#include "shading_system.h"
#include "PixelShaderTextureCommon.h"

Texture2D    texture0 : register(t0);
SamplerState sampler0 : register(s0);

// pos3D/Depth buffer has the following coords:
// X+: Right
// Y+: Up
// Z+: Away from the camera
// (0,0,0) is the camera center, (0,0,Z) is the center of the screen

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
	float4 texelColor = texture0.Sample(sampler0, input.tex);
	float  alpha = texelColor.a;
	float3 HSV = RGBtoHSV(texelColor.rgb);
	if (special_control == SPECIAL_CONTROL_BLACK_TO_ALPHA)
		alpha = HSV.z;

	float3 diffuse = lerp(input.color.xyz, 1.0, fDisableDiffuse);
	float3 P = input.pos3D.xyz;
	float  SSAOAlpha = saturate(min(alpha - fSSAOAlphaOfs, fPosNormalAlpha));
	// Zero-out the bloom mask.
	output.bloom = float4(0, 0, 0, alpha);
	output.color = texelColor;
	output.pos3D = float4(P, SSAOAlpha);
	output.ssMask = 0;
	

	// hook_normals code:
	float3 N = normalize(input.normal.xyz * 2.0 - 1.0);
	N.y = -N.y; // Invert the Y axis, originally Y+ is down
	N.z = -N.z;
	output.normal = float4(N, SSAOAlpha);

	// ssaoMask.r: Material
	// ssaoMask.g: Glossiness
	// ssaoMask.b: Specular Intensity
	output.ssaoMask = float4(fSSAOMaskVal, fGlossiness, fSpecInt, alpha);
	// SS Mask: Normal Mapping Intensity, Specular Value, Shadeless
	output.ssMask = float4(fNMIntensity, fSpecVal, fAmbient, alpha);
	
	// bloom
	if (HSV.z >= 0.8) {
		float bloom_alpha = saturate(fBloomStrength);
		diffuse = 1.0;
		output.bloom = float4(fBloomStrength * texelColor.rgb, alpha);
		//output.ssaoMask.r = SHADELESS_MAT;
		output.ssMask.b = bloom_alpha;
		//output.ssaoMask.ga = 1; // Maximum glossiness on light areas
		output.ssaoMask.a = bloom_alpha;
		//output.ssaoMask.b = 0.15; // Low spec intensity
		output.ssaoMask.b = 0.15 * bloom_alpha; // Low spec intensity
	}
	output.color = float4(brightness * diffuse * texelColor.xyz, texelColor.w);
	return output;
}
