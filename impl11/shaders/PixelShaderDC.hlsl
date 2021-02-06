// Copyright (c) 2019, 2020 Leo Reyes
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

// HUD foreground
Texture2D    texture1 : register(t1);
SamplerState sampler1 : register(s1);

// HUD text
Texture2D    texture2 : register(t2);
SamplerState sampler2 : register(s2);

// When the Dynamic Cockpit is active:
// texture0 == cover texture and
// texture1 == HUD offscreen buffer
// texture2 == Text buffer

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

float4 uintColorToFloat4(uint color, out float intensity, out float text_alpha_override, out float obj_alpha_override, out bool dc_bloom) {
	float4 result = float4(
		((color >> 16) & 0xFF) / 255.0,  // R 0xFF0000
		((color >>  8) & 0xFF) / 255.0,  // G 0x00FF00
		 (color        & 0xFF) / 255.0,  // B 0x0000FF
		                             1); // Alpha
	// The alpha component encodes more information:
	// bits 0-1: an integer in the range 0..3 that specifies the intensity. intensity = bits[0..1] + 1
	// bit 2: Enable/Disable text layer (on/off switch)
	// bit 3: OBJ alpha override
	// bit 4: Enable bloom in DC elements
	//intensity = ((color >> 24) & 0xFF) / 64.0;
	uint temp = (color >> 24) & 0xFF;
	intensity			= (temp & 0x03) + 1.0;
	text_alpha_override = (float)((temp & 0x04) >> 2);
	obj_alpha_override  = (float)((temp & 0x08) >> 3);
	dc_bloom			= (bool)((temp & 0x10) >> 4);
	return result;
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
	// coverColor/texelColor is the cover texture
	float2 UV = (input.tex + Offset.xy) * float2(AspectRatio, 1);
	if (Clamp) UV = saturate(UV);
	float4 coverColor = AuxColor * texture0.Sample(sampler0, UV);
	float coverAlpha = coverColor.w; // alpha of the cover texture
	float3 HSV = RGBtoHSV(coverColor.rgb);
	if (special_control == SPECIAL_CONTROL_BLACK_TO_ALPHA)
		coverAlpha = HSV.z;

	// DEBUG: Make the cover texture transparent to show the DC contents clearly
	//const float coverAlpha = 0.0;
	// DEBUG
	float3 diffuse = lerp(input.color.xyz, 1.0, fDisableDiffuse);
	//output.diffuse = float4(diffuse, 1);
	// Zero-out the bloom mask.
	output.bloom = float4(0, 0, 0, 0);
	output.color = coverColor;
	output.pos3D = float4(input.pos3D.xyz, 1);

	// hook_normals code:
	float3 N = normalize(input.normal.xyz * 2.0 - 1.0);
	N.y = -N.y; // Invert the Y axis, originally Y+ is down
	N.z = -N.z;
	output.normal = float4(N, 1);

	//output.ssaoMask.r = PLASTIC_MAT;
	//output.ssaoMask.g = DEFAULT_GLOSSINESS; // Default glossiness
	//output.ssaoMask.b = DEFAULT_SPEC_INT;   // Default spec intensity
	//output.ssaoMask.a = 0.0;
	output.ssaoMask = float4(fSSAOMaskVal, fGlossiness, fSpecInt, coverAlpha);

	// SS Mask: Normal Mapping Intensity (overriden), Specular Value, Shadeless
	output.ssMask = float4(fNMIntensity, fSpecVal, 0.0, 0.0);

	// Render the captured Dynamic Cockpit buffer into the cockpit destination textures. 
	// We assume this shader will be called iff DynCockpitSlots > 0

	// DEBUG: Display uvs as colors. Some meshes have UVs beyond the range [0..1]
		//output.color = float4(frac(input.tex.xy), 0, 1); // DEBUG: Display the uvs as colors
		//output.ssaoMask = float4(SHADELESS_MAT, 0, 0, 1);
		//output.ssMask = 0;
		//return output;
	// DEBUG
		//return 0.7*hud_texelColor + 0.3*texelColor; // DEBUG DEBUG DEBUG!!! Remove this later! This helps position the elements easily
	

	// HLSL packs each element in an array in its own 4-vector (16-byte) row. So src[0].xy is the
	// upper-left corner of the box and src[0].zw is the lower-right corner. The same applies to
	// dst uv coords

	// Fix UVs that are greater than 1 or negative.
	input.tex = frac(input.tex);
	if (input.tex.x < 0.0) input.tex.x += 1.0;
	if (input.tex.y < 0.0) input.tex.y += 1.0;
	float intensity, text_alpha_override = 1.0, obj_alpha_override = 1.0;
	bool dc_bloom = false;
	float4 hud_texelColor = uintColorToFloat4(getBGColor(0), intensity, text_alpha_override, obj_alpha_override, dc_bloom);
	//[unroll] unroll or loop?
	[loop]
	for (uint i = 0; i < DynCockpitSlots; i++) {
		float2 delta = dst[i].zw - dst[i].xy;
		float2 s = (input.tex - dst[i].xy) / delta;
		float2 dyn_uv = lerp(src[i].xy, src[i].zw, s);
		float4 bgColor = uintColorToFloat4(getBGColor(i), intensity, text_alpha_override, obj_alpha_override, dc_bloom);

		if (all(dyn_uv >= src[i].xy) && all(dyn_uv <= src[i].zw))
		{
			// Sample the dynamic cockpit texture:
			hud_texelColor = obj_alpha_override * texture1.Sample(sampler1, dyn_uv);
			// Sample the text texture and fix the alpha:
			float4 texelText = texture2.Sample(sampler2, dyn_uv);
			//float textAlpha = text_alpha_override * saturate(3.25 * dot(0.333, texelText.rgb));
			float textAlpha = text_alpha_override * saturate(10.0 * dot(float3(0.33, 0.5, 0.16), texelText.rgb));
			// Blend the text with the DC buffer
			hud_texelColor.rgb = lerp(hud_texelColor.rgb, texelText.rgb, textAlpha);
			hud_texelColor.w = saturate(dc_brightness * max(hud_texelColor.w, textAlpha));
			hud_texelColor = saturate(intensity * hud_texelColor);

			const float hud_alpha = hud_texelColor.w;
			// DEBUG: Display the source UVs
			//const float hud_alpha = 1.0;
			//hud_texelColor = float4(dyn_uv, 0, 1);
			// DEBUG
			
			// Add the background color to the dynamic cockpit display:
			hud_texelColor = lerp(bgColor, hud_texelColor, hud_alpha);
		}
	}
	// At this point hud_texelColor has the color from the offscreen HUD buffer blended with bgColor

	// Blend the offscreen buffer HUD texture with the cover texture and go shadeless where transparent.
	// Also go shadeless where the cover texture is bright enough and mark that in the bloom mask.
	if (bUseCoverTexture > 0) {
		// We don't have an alpha overlay texture anymore; but we can fake it by disabling shading
		// on areas with a high lightness value

		// coverColor is the cover_texture right now
		//float3 HSV = RGBtoHSV(coverColor.xyz);
		float brightness = ct_brightness;
		// The cover texture is bright enough, go shadeless and make it brighter
		if (HSV.z * coverAlpha >= 0.8) {
			diffuse = 1;
			// Increase the brightness:
			//HSV = RGBtoHSV(coverColor.xyz); // Redundant
			HSV.z *= 1.2;
			coverColor.xyz = HSVtoRGB(HSV);
			output.bloom = float4(fBloomStrength * coverColor.xyz, 1);
			brightness = 1.0;
			output.ssaoMask.r  = SHADELESS_MAT;
			output.ssaoMask.ga = 1; // Maximum glossiness on light areas?
			output.ssaoMask.b  = 0.15; // Low spec intensity
		}
		// Display the dynamic cockpit element only where the texture cover is transparent:
		// In 32-bit mode, the cover textures appear brighter, we should probably dim them, 
		// that's what the brightness setting below is for:
		coverColor = lerp(hud_texelColor, brightness * coverColor, coverAlpha);
		output.bloom = lerp(0.0, output.bloom, coverAlpha);
		// The diffuse value will be 1 (shadeless) wherever the cover texture is transparent:
		diffuse = lerp(1.0, diffuse, coverAlpha);
		// ssaoMask: SSAOMask/Material, Glossiness x 128, SpecInt, alpha
		// ssMask: NMIntensity, SpecValue, unused
		// DC areas are shadeless, have high glossiness and low spec intensity
		// if coverAlpha is 1, this is the cover texture
		// if coverAlpha is 0, this is the hole in the cover texture
		output.ssaoMask.rgb = lerp(float3(SHADELESS_MAT, 1.0, 0.15), output.ssaoMask.rgb, coverAlpha);
		output.ssMask.rg    = lerp(float2(0.0, 1.0), output.ssMask.rg, coverAlpha); // Normal Mapping intensity, Specular Value
		output.ssaoMask.a   = max(output.ssaoMask.a, (1.0 - coverAlpha));
		output.ssMask.a     = output.ssaoMask.a; // Already clamped in the previous line
	}
	else {
		coverColor = hud_texelColor;
		coverAlpha = coverColor.w;
		diffuse = 1.0;
		// SSAOMask, Glossiness x 128, Spec_Intensity, alpha
		output.ssaoMask = float4(SHADELESS_MAT, 1, 0.15, 1);
		output.ssMask = float4(0.0, 1.0, 0.0, 1.0); // No NM, White Spec Val, unused
	}
	
	// At this point, coverColor is the blended cover texture (if it exists) and the HUD contents
	if (dc_bloom) {
		// coverColor may have changed, we need to convert to HSV again
		float3 HSV = RGBtoHSV(coverColor.xyz);
		if (HSV.z >= 0.8) {
			diffuse = 1.0;
			output.bloom = float4(fBloomStrength * coverColor.xyz, 1);
			output.ssaoMask.r = SHADELESS_MAT;
			output.ssaoMask.ga = 1; // Maximum glossiness on light areas
			output.ssaoMask.b = 0.15; // Low spec intensity
		}
	}

	output.color = float4(diffuse * coverColor.xyz, coverAlpha);
	if (bInHyperspace) output.color.a = 1.0;

	// Text DC elements can be made to float inside the cockpit. In that case, we might want
	// them to be transparent and this code achieves that.
	if (transparent) {
		float alpha = 4.0 * dot(0.333, output.color.rgb);
		output.color.a    = alpha;
		output.bloom      = 0;
		output.pos3D      = 0;
		output.normal     = 0;
		output.ssaoMask.a = alpha;
		output.ssMask.a   = alpha;
	}
	return output;
}
