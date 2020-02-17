// Copyright (c) 2019 Leo Reyes
// Licensed under the MIT license. See LICENSE.txt
// This shader should only be called for destination textures when DC is
// enabled. For regular textures or when the Dynamic Cockpit is disabled,
// use "PixelShader.hlsl" instead.
#include "HSV.h"
#include "shader_common.h"
#include "shading_system.h"

Texture2D    texture0 : register(t0);
SamplerState sampler0 : register(s0);

Texture2D    texture1 : register(t1);
SamplerState sampler1 : register(s1);

// When the Dynamic Cockpit is active:
// texture0 == cover texture and
// texture1 == HUD offscreen buffer

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
	uint bInHyperspace;

	float fBloomStrength;		// General multiplier for the bloom effect
	float fPosNormalAlpha;		// (Ignored) Override for pos3D and normal output alpha
	float fSSAOMaskVal;			// (Ignored) SSAO mask value
	float fSSAOAlphaOfs;			// (Ignored) Additional offset substracted from alpha when rendering SSAO. Helps prevent halos around transparent objects.

	uint bIsShadeless;
	float fGlossiness, fSpecInt, fNMIntensity;
	// 64 bytes

	float fSpecVal, fDisableDiffuse, unusedPS2, unusedPS3;
	// 80 bytes
};

// DCPixelShaderCBuffer
cbuffer ConstantBuffer : register(b1)
{
	float4 src[MAX_DC_COORDS_PER_TEXTURE];		  // HLSL packs each element in an array in its own 4-vector (16 bytes) slot, so .xy is src0 and .zw is src1
	float4 dst[MAX_DC_COORDS_PER_TEXTURE];
	uint4 bgColor[MAX_DC_COORDS_PER_TEXTURE / 4]; // Background colors to use for the dynamic cockpit, this divide by 4 is because HLSL packs each elem in a 4-vector,
									  // So each elem here is actually 4 bgColors.

	float ct_brightness;				  // Cover texture brightness. In 32-bit mode the cover textures have to be dimmed.
	float unused1, unused2, unused3;
};

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
	//if (N.z < 0.0) N.z = 0.0; // Avoid vectors pointing away from the view
	// Do not flip N.z -- that causes flat unoccluded surfaces to be shaded in SSAO
	output.normal = float4(N, 1);

	//output.ssaoMask = 0;
	output.ssaoMask = float4(fSSAOMaskVal, fGlossiness, fSpecInt, alpha);
	//output.diffuse = input.color;

	// SS Mask: Normal Mapping Intensity (overriden), Specular Value, unused
	output.ssMask = float4(fNMIntensity, fSpecVal, 0.0, 0.0);

	// No DynCockpitSlots; but we're using a cover texture anyway. Clear the holes.
	// The code returns a color from this path
	//else if (bUseCoverTexture > 0) {
	// We assume the caller will set this pixel shader iff DynCockpitSlots == 0 && bUseCoverTexture > 0
	
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
		//output.ssaoMask = 1;
		output.ssaoMask.r  = SHADELESS_MAT;
		output.ssaoMask.ga = 1; // Maximum glossiness on light areas?
		output.ssaoMask.b  = 0.15; // Low spec intensity
	}
	
	texelColor = lerp(hud_texelColor, brightness * texelColor, alpha);
	// The diffuse value will be 1 (shadeless) wherever the cover texture is transparent:
	diffuse = lerp(1.0, diffuse, alpha);
	output.color = float4(diffuse * texelColor.xyz, alpha);
	output.bloom = lerp(float4(0, 0, 0, 0), output.bloom, alpha);

	// ssaoMask: SSAOMask/Material, Glossiness x 128, SpecInt, alpha
	// ssMask: NMIntensity, SpecValue, unused
	// DC areas are shadeless, have high glossiness and low spec intensity
	// if alpha is 1, this is the cover texture
	// if alpha is 0, this is the hole in the cover texture
	output.ssaoMask.rgb = lerp(float3(SHADELESS_MAT, 1.0, 0.15), output.ssaoMask.rgb, alpha);
	output.ssMask.rg    = lerp(float2(0.0, 1.0), output.ssMask.rg, alpha); // Normal Mapping intensity, Specular Value
	output.ssaoMask.a   = max(output.ssaoMask.a, (1 - alpha));
	output.ssMask.a     = output.ssaoMask.a; // Already clamped in the previous line
	return output;

	//output.color = float4(brightness * diffuse * texelColor.xyz, texelColor.w);
	//return output;
}
