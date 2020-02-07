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

// If bRenderHUD is set: Is this used anymore???
// texture0 == HUD foreground
// texture1 == HUD background

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

	uint bIsBackground, unusedPS0, unusedPS1, unusedPS2;
};

cbuffer ConstantBuffer : register(b1)
{
	float4 src[MAX_DC_COORDS];		  // HLSL packs each element in an array in its own 4-vector (16 bytes) slot, so .xy is src0 and .zw is src1
	float4 dst[MAX_DC_COORDS];
	uint4 bgColor[MAX_DC_COORDS / 4]; // Background colors to use for the dynamic cockpit, this divide by 4 is because HLSL packs each elem in a 4-vector,
									  // So each elem here is actually 4 bgColors.

	float ct_brightness;				  // Cover texture brightness. In 32-bit mode the cover textures have to be dimmed.
	float unused1, unused2, unused3;
};

float4 uintColorToFloat4(uint color) {
	return float4(
		((color >> 16) & 0xFF) / 255.0,
		((color >> 8) & 0xFF) / 255.0,
		(color & 0xFF) / 255.0,
		1);
}

/*
float4 uintColorToFloat4(uint color) {
	float r, g, b;
	b = (color % 256) / 255.0;
	color = color / 256;
	g = (color % 256) / 255.0;
	color = color / 256;
	r = (color % 256) / 255.0;
	return float4(r, g, b, 1);
}
*/

uint getBGColor(uint i) {
	uint idx = i / 4;
	uint sub_idx = i % 4;
	return bgColor[idx][sub_idx];
}

PixelShaderOutput main(PixelShaderInput input)
{
	PixelShaderOutput output;
	float4 texelColor = texture0.Sample(sampler0, input.tex); // texelColor is the cover texture
	float alpha = texelColor.w; // alpha of the cover texture
	float3 diffuse = input.color.xyz;
	//output.diffuse = float4(diffuse, 1);
	// Zero-out the bloom mask.
	output.bloom = float4(0, 0, 0, 0);
	output.color = texelColor;

	float3 P = input.pos3D.xyz;
	output.pos3D = float4(P, 1);

	// Original code:
	//float3 N = normalize(cross(ddx(P), ddy(P)));
	//if (N.z < 0.0) N.z = 0.0; // Avoid vectors pointing away from the view
	// Don't flip N.z -- that causes more artifacts: flat unoccluded surfaces become shaded in SSAO

	// hook_normals code:
	float3 N = normalize(input.normal.xyz * 2.0 - 1.0);
	N.y = -N.y; // Invert the Y axis, originally Y+ is down
	N.z = -N.z;
	
	output.normal = float4(N, 1);

	output.ssaoMask.r = PLASTIC_MAT;
	output.ssaoMask.g = DEFAULT_GLOSSINESS; // Default glossiness
	output.ssaoMask.b = DEFAULT_SPEC_INT;   // Default spec intensity
	output.ssaoMask.a = 0.0;

	// Render the captured Dynamic Cockpit buffer into the cockpit destination textures. 
	// We assume this shader will be called iff DynCockpitSlots > 0

	// DEBUG: Display uvs as colors. Some meshes have UVs beyond the range [0..1]
		//if (input.tex.x > 1.0) 	return float4(1, 0, 1, 1);
		//if (input.tex.y > 1.0) 	return float4(0, 0, 1, 1);
		//return float4(input.tex.xy, 0, 1); // DEBUG: Display the uvs as colors
		//return 0.7*hud_texelColor + 0.3*texelColor; // DEBUG DEBUG DEBUG!!! Remove this later! This helps position the elements easily

	// HLSL packs each element in an array in its own 4-vector (16-byte) row. So src[0].xy is the
	// upper-left corner of the box and src[0].zw is the lower-right corner. The same applies to
	// dst uv coords

	// Fix UVs that are greater than 1. I wonder if I should also fix negative values?
	input.tex = frac(input.tex);
	if (input.tex.x < 0.0) input.tex.x += 1.0;
	if (input.tex.y < 0.0) input.tex.y += 1.0;
	float4 hud_texelColor = uintColorToFloat4(getBGColor(0));
	//[unroll] unroll or loop?
	[loop]
	for (uint i = 0; i < DynCockpitSlots; i++) {
		float2 delta = dst[i].zw - dst[i].xy;
		float2 s = (input.tex - dst[i].xy) / delta;
		float2 dyn_uv = lerp(src[i].xy, src[i].zw, s);

		if (dyn_uv.x >= src[i].x && dyn_uv.x <= src[i].z &&
			dyn_uv.y >= src[i].y && dyn_uv.y <= src[i].w)
		{
			// Sample the dynamic cockpit texture:
			hud_texelColor = texture1.Sample(sampler1, dyn_uv);
			float hud_alpha = hud_texelColor.w;
			// Add the background color to the dynamic cockpit display:
			hud_texelColor = lerp(uintColorToFloat4(getBGColor(i)), hud_texelColor, hud_alpha);
		}
	}
	// At this point hud_texelColor has the color from the offscreen HUD buffer blended with bgColor

	// Blend the offscreen buffer HUD texture with the cover texture and go shadeless where transparent.
	// Also go shadless where the cover texture is bright enough.
	if (bUseCoverTexture > 0) {
		// We don't have an alpha overlay texture anymore; but we can fake it by disabling shading
		// on areas with a high lightness value

		// texelColor is the cover_texture right now
		float3 HSV = RGBtoHSV(texelColor.xyz);
		float brightness = ct_brightness;
		// The cover texture is bright enough, go shadeless and make it brighter
		if (HSV.z * alpha >= 0.8) {
			diffuse = 1;
			// Increase the brightness:
			HSV = RGBtoHSV(texelColor.xyz);
			HSV.z *= 1.2;
			texelColor.xyz = HSVtoRGB(HSV);
			output.bloom = float4(fBloomStrength * texelColor.xyz, 1);
			brightness = 1.0;
			output.ssaoMask.r  = SHADELESS_MAT;
			output.ssaoMask.ga = 1; // Maximum glossiness on light areas?
			output.ssaoMask.b  = 0.15; // Low spec intensity
		}
		// Display the dynamic cockpit texture only where the texture cover is transparent:
		// In 32-bit mode, the cover textures appear brighter, we should probably dim them, 
		// that's what the brightness setting below is for:
		texelColor = lerp(hud_texelColor, brightness * texelColor, alpha);
		output.bloom = lerp(float4(0, 0, 0, 0), output.bloom, alpha);
		// The diffuse value will be 1 (shadeless) wherever the cover texture is transparent:
		diffuse = lerp(float3(1, 1, 1), diffuse, alpha);
		// SSAOMask, Glossiness x 128, ?, alpha
		output.ssaoMask.r = lerp(SHADELESS_MAT, PLASTIC_MAT, alpha);
		output.ssaoMask.a = max(output.ssaoMask.a, (1 - alpha));
		// if alpha is 1, this is the cover texture --> Glossiness = DEFAULT_GLOSSINESS
		// if alpha is 0, this is the hole in the cover texture --> Maximum glossiness
		output.ssaoMask.g = lerp(1.00, DEFAULT_GLOSSINESS, alpha);
		output.ssaoMask.b = lerp(0.15, DEFAULT_SPEC_INT, alpha); // Low spec intensity
	}
	else {
		texelColor = hud_texelColor;
		diffuse = float3(1, 1, 1);
		// SSAOMask, Glossiness x 128, Spec_Intensity, alpha
		output.ssaoMask = float4(SHADELESS_MAT, 1, 0.15, 1);
	}
	output.color = float4(diffuse * texelColor.xyz, texelColor.w);
	if (bInHyperspace) output.color.a = 1.0;
	return output;
}
