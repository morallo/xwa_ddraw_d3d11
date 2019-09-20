// Copyright (c) 2019 Leo Reyes
// Licensed under the MIT license. See LICENSE.txt
// This shader should only be called for destination textures when DC is
// enabled. For regular textures or when the Dynamic Cockpit is disabled,
// use "PixelShader.hlsl" instead.

Texture2D    texture0 : register(t0);
SamplerState sampler0 : register(s0);

Texture2D    texture1 : register(t1);
SamplerState sampler1 : register(s1);

// When the Dynamic Cockpit is active:
// texture0 == cover texture and
// texture1 == HUD offscreen buffer

// If bRenderHUD is set:
// texture0 == HUD foreground
// texture1 == HUD background

struct PixelShaderInput
{
	float4 pos : SV_POSITION;
	float4 color : COLOR0;
	float2 tex : TEXCOORD;
};

struct PixelShaderOutput
{
	float4 color : SV_TARGET0;
	float4 bloom : SV_TARGET1;
};

cbuffer ConstantBuffer : register(b0)
{
	float brightness;		// Used to dim some elements to prevent the Bloom effect -- mostly for ReShade compatibility
	uint DynCockpitSlots;	// How many DC slots will be used. This setting was "bShadeless" previously
	uint bUseCoverTexture;	// When set, use the first texture as cover texture for the dynamic cockpit
	uint bIsHyperspaceAnim;
	// 16 bytes

	uint bIsLaser;					// 1 for Laser objects, setting this to 2 will make them brighter (intended for 32-bit mode)
	uint bIsLightTexture;			// 1 if this is a light texture, 2 will make it brighter (intended for 32-bit mode)
	uint bIsEngineGlow;				// 1 if this is an engine glow textures, 2 will make it brighter (intended for 32-bit mode)
	uint bIsHyperspaceStreak;

	float fBloomStrength;			// General multiplier for the bloom effect
};

#define MAX_DC_COORDS 8
cbuffer ConstantBuffer : register(b1)
{
	float4 src[MAX_DC_COORDS];		  // HLSL packs each element in an array in its own 4-vector (16 bytes) slot, so .xy is src0 and .zw is src1
	float4 dst[MAX_DC_COORDS];
	uint4 bgColor[MAX_DC_COORDS / 4]; // Background colors to use for the dynamic cockpit, this divide by 4 is because HLSL packs each elem in a 4-vector,
									  // So each elem here is actually 4 bgColors.

	float ct_brightness;				  // Cover texture brightness. In 32-bit mode the cover textures have to be dimmed.
	// unused1, unused2, unused3
};

// From http://www.chilliant.com/rgb2hsv.html
static float Epsilon = 1e-10;

float3 HUEtoRGB(in float H)
{
	float R = abs(H * 6 - 3) - 1;
	float G = 2 - abs(H * 6 - 2);
	float B = 2 - abs(H * 6 - 4);
	return saturate(float3(R, G, B));
}

float3 RGBtoHCV(in float3 RGB)
{
	// Based on work by Sam Hocevar and Emil Persson
	float4 P = (RGB.g < RGB.b) ? float4(RGB.bg, -1.0, 2.0 / 3.0) : float4(RGB.gb, 0.0, -1.0 / 3.0);
	float4 Q = (RGB.r < P.x) ? float4(P.xyw, RGB.r) : float4(RGB.r, P.yzx);
	float C = Q.x - min(Q.w, Q.y);
	float H = abs((Q.w - Q.y) / (6 * C + Epsilon) + Q.z);
	return float3(H, C, Q.x);
}

float3 RGBtoHSV(in float3 RGB)
{
	float3 HCV = RGBtoHCV(RGB);
	float S = HCV.y / (HCV.z + Epsilon);
	return float3(HCV.x, S, HCV.z);
}

float3 HSVtoRGB(in float3 HSV)
{
	float3 RGB = HUEtoRGB(HSV.x);
	return ((RGB - 1) * HSV.y + 1) * HSV.z;
}

/*
float4 uintColorToFloat4(uint color) {
	return float4(
		((color >> 16) & 0xFF) / 255.0,
		((color >> 8) & 0xFF) / 255.0,
		(color & 0xFF) / 255.0,
		1);
}
*/

float4 uintColorToFloat4(uint color) {
	float r, g, b;
	b = (color % 256) / 255.0;
	color = color / 256;
	g = (color % 256) / 255.0;
	color = color / 256;
	r = (color % 256) / 255.0;
	return float4(r, g, b, 1);
}

uint getBGColor(uint i) {
	uint idx = i / 4;
	uint sub_idx = i % 4;
	return bgColor[idx][sub_idx];
}

PixelShaderOutput main(PixelShaderInput input)
{
	PixelShaderOutput output;
	float4 texelColor = texture0.Sample(sampler0, input.tex);
	float alpha = texelColor.w;
	float3 diffuse = input.color.xyz;
	// Zero-out the bloom mask.
	output.bloom = float4(0, 0, 0, 0);
	output.color = texelColor;

	// Render the Dynamic Cockpit captured buffer into the cockpit destination textures. 
	// The code returns a color from this path
	if (DynCockpitSlots > 0) {
		// DEBUG: Display uvs as colors. Some meshes have UVs beyond the range [0..1]
		//if (input.tex.x > 1.0) 	return float4(1, 0, 1, 1);
		//if (input.tex.y > 1.0) 	return float4(0, 0, 1, 1);
		//return float4(input.tex.xy, 0, 1); // DEBUG: Display the uvs as colors
		//return 0.7*hud_texelColor + 0.3*texelColor; // DEBUG DEBUG DEBUG!!! Remove this later! This helps position the elements easily

		// HLSL packs each element in an array in its own 4-vector (16-byte) row. So src[0].xy is the
		// upper-left corner of the box and src[0].zw is the lower-right corner. The same applies to
		// dst uv coords

		float4 hud_texelColor = uintColorToFloat4(getBGColor(0));
		[unroll]
		for (uint i = 0; i < DynCockpitSlots; i++) {
			float2 delta = dst[i].zw - dst[i].xy;
			float2 s = (input.tex - dst[i].xy) / delta;
			float2 dyn_uv = lerp(src[i].xy, src[i].zw, s);

			if (dyn_uv.x >= src[i].x && dyn_uv.x <= src[i].z &&
				dyn_uv.y >= src[i].y && dyn_uv.y <= src[i].w)
			{
				// Sample the dynamic cockpit texture:
				hud_texelColor = texture1.Sample(sampler1, dyn_uv); // "ct" is for "cover_texture"
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
			if (HSV.z * alpha >= 0.8) {
				// The cover texture is bright enough, go shadeless and make it brighter
				diffuse = float3(1, 1, 1);
				// Increase the brightness:
				HSV = RGBtoHSV(texelColor.xyz);
				HSV.z *= 1.2;
				texelColor.xyz = HSVtoRGB(HSV);
				output.bloom = float4(fBloomStrength * texelColor.xyz, 1);
				brightness = 1.0;
			}
			// Display the dynamic cockpit texture only where the texture cover is transparent:
			// In 32-bit mode, the cover textures appear brighter, we should probably dim them, that's what the 0.8 below is for:
			texelColor = lerp(hud_texelColor, brightness * texelColor, alpha);
			output.bloom = lerp(float4(0, 0, 0, 0), output.bloom, alpha);
			// The diffuse value will be 1 (shadeless) wherever the cover texture is transparent:
			diffuse = lerp(float3(1, 1, 1), diffuse, alpha);
		}
		else {
			texelColor = hud_texelColor;
			diffuse = float3(1, 1, 1);
		}
		output.color = float4(diffuse * texelColor.xyz, texelColor.w);
		return output;
	}
	// No DynCockpitSlots; but we're using a cover texture anyway. Clear the holes.
	// The code returns a color from this path
	else if (bUseCoverTexture > 0) {
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
		}
		texelColor = lerp(hud_texelColor, brightness * texelColor, alpha);
		output.color = float4(diffuse * texelColor.xyz, alpha);
		return output;
	}

	output.color = float4(brightness * diffuse * texelColor.xyz, texelColor.w);
	return output;
}
