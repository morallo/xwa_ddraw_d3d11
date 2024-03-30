// Copyright (c) 2019 Leo Reyes
// Licensed under the MIT license. See LICENSE.txt
// This shader should only be called for destination textures when DC is
// enabled. For regular textures or when the Dynamic Cockpit is disabled,
// use "PixelShader.hlsl" instead.
#include "HSV.h"
#include "shader_common.h"
#include "shading_system.h"
#include "PixelShaderTextureCommon.h"
#include "DC_common.h"

// Cover texture
Texture2D    texture0 : register(t0);
SamplerState sampler0 : register(s0);

// HUD offscreen buffer
Texture2D    texture1 : register(t1);
SamplerState sampler1 : register(s1);

// When the Dynamic Cockpit is active:
// texture0 == cover texture and
// texture1 == HUD offscreen buffer

/*
// Old PixelShaderInput (pre-D3DRendererHook):
struct PixelShaderInput
{
	float4 pos    : SV_POSITION;
	float4 color  : COLOR0;
	float2 tex    : TEXCOORD0;
	float4 pos3D  : COLOR1;
	float4 normal : NORMAL;
};
*/

// New PixelShaderInput needed for the D3DRendererHook
struct PixelShaderInput
{
	float4 pos		: SV_POSITION;
	float4 pos3D		: COLOR1;
	float4 normal	: NORMAL;
	float2 tex		: TEXCOORD;
	//float4 color  : COLOR0;
};

struct PixelShaderOutput
{
	float4 color		: SV_TARGET0;
	float4 bloom		: SV_TARGET1;
	float4 pos3D		: SV_TARGET2;
	float4 normal	: SV_TARGET3;
	float4 ssaoMask : SV_TARGET4;
	float4 ssMask	: SV_TARGET5;
};

PixelShaderOutput main(PixelShaderInput input)
{
	PixelShaderOutput output;
	float2 UV = input.tex * float2(AspectRatio, 1) + Offset.xy;
	if (Clamp) UV = saturate(UV);
	float4 texelColor = AuxColor * texture0.Sample(sampler0, UV);
	float alpha = texelColor.w;
	float3 HSV = RGBtoHSV(texelColor.rgb); // texelColor is the cover texture
	uint ExclusiveMask = special_control & SPECIAL_CONTROL_EXCLUSIVE_MASK;
	if (ExclusiveMask == SPECIAL_CONTROL_BLACK_TO_ALPHA)
		alpha = HSV.z;
	// Zero-out the bloom mask.
	output.bloom = float4(0, 0, 0, 0);
	output.color = texelColor;

	float3 P = input.pos3D.xyz;
	output.pos3D = float4(P, 1);

	float3 N = normalize(input.normal.xyz);
	N.y = -N.y; // Invert the Y axis, originally Y+ is down
	N.z = -N.z;
	output.normal = float4(N, 1);

	output.ssaoMask = float4(fSSAOMaskVal, fGlossiness, fSpecInt, alpha);
	// SS Mask: Glass, Specular Value, Shadeless
	output.ssMask = float4(0, fSpecVal, 0.0, 0.0);

	// No DynCockpitSlots; but we're using a cover texture anyway. Clear the holes.
	// The code returns a color from this path
	//else if (bUseCoverTexture > 0) {
	// We assume the caller will set this pixel shader iff DynCockpitSlots == 0 && bUseCoverTexture > 0
	
	// texelColor is the cover_texture right now
	//float3 HSV = RGBtoHSV(texelColor.xyz);
	float4 hud_texelColor = float4(0, 0, 0, 1);
	float brightness = ct_brightness;
	if (HSV.z * alpha >= 0.8) {
		// The cover texture is bright enough, go shadeless and make it brighter
		// Increase the brightness:
		HSV.z *= 1.2;
		texelColor.xyz = HSVtoRGB(HSV);
		output.bloom = float4(fBloomStrength * texelColor.xyz, 1);
		brightness = 1.0;
		output.ssaoMask.r  = 0;
		output.ssaoMask.ga = 1; // Maximum glossiness on light areas?
		output.ssaoMask.b  = 0.15; // Low spec intensity
		output.ssMask.b    = 1; // Shadeless material
	}
	
	texelColor = lerp(hud_texelColor, brightness * texelColor, alpha);
	// The diffuse value will be 1 (shadeless) wherever the cover texture is transparent:
	//diffuse = lerp(1.0, diffuse, alpha);
	output.color = float4(/* diffuse * */ texelColor.xyz, alpha);
	output.bloom = lerp(float4(0, 0, 0, 0), output.bloom, alpha);

	// ssaoMask: SSAOMask/Material, Glossiness x 128, SpecInt, alpha
	// ssMask: NMIntensity, SpecValue, unused
	// DC areas are shadeless, have high glossiness and low spec intensity
	// if alpha is 1, this is the cover texture
	// if alpha is 0, this is the hole in the cover texture
	output.ssaoMask.rgb = lerp(float3(0, 1.0, 0.15), output.ssaoMask.rgb, alpha);
	output.ssMask.rg    = lerp(float2(0.0, 1.0), output.ssMask.rg, alpha); // Normal Mapping intensity, Specular Value
	output.ssaoMask.a   = max(output.ssaoMask.a, (1 - alpha));
	output.ssMask.a     = output.ssaoMask.a; // Already clamped in the previous line
	output.ssMask.b     = 1.0 - alpha;
	//if (bInHyperspace) output.color.a = 1.0; // Hyperspace transparency fix
	// Transparency fix -- everywhere
	output.color.a = 1;
	return output;
}
