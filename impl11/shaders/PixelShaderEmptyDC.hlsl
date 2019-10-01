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
	float2 tex : TEXCOORD0;
	float4 pos3D : COLOR1;
};

struct PixelShaderOutput
{
	float4 color  : SV_TARGET0;
	float4 bloom  : SV_TARGET1;
	float4 pos3D  : SV_TARGET2;
	float4 normal : SV_TARGET3;
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

float4 uintColorToFloat4(uint color) {
	return float4(
		((color >> 16) & 0xFF) / 255.0,
		((color >> 8) & 0xFF) / 255.0,
		(color & 0xFF) / 255.0,
		1);
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

	float3 P = input.pos3D.xyz;
	output.pos3D = float4(P, 1);

	float3 N = normalize(cross(ddx(P), ddy(P)));
	output.normal = float4(N, 1);

	// No DynCockpitSlots; but we're using a cover texture anyway. Clear the holes.
	// The code returns a color from this path
	//else if (bUseCoverTexture > 0) {
	// We assume the caller will set this pixel shader iff DynCockpitSlots == 0 && bUseCoverTexture > 0
	{
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
