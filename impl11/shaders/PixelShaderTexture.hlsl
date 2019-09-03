// Copyright (c) 2014 Jérémy Ansel
// Licensed under the MIT license. See LICENSE.txt
// Extended for VR by Leo Reyes (c) 2019

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
	float brightness;			// Used to dim some elements to prevent the Bloom effect -- mostly for ReShade compatibility
	uint DynCockpitSlots;		// How many DC slots will be used. This setting was "bShadeless" previously
	uint bUseCoverTexture;		// When set, use the first texture as cover texture for the dynamic cockpit
	uint bIsHyperspaceAnim;		// 1 if we're rendering the hyperspace animation
	// 16 bytes

	uint bIsLaser;				// 1 for Laser objects, setting this to 2 will make them brighter (intended for 32-bit mode)
	uint bIsLightTexture;		// 1 if this is a light texture, 2 will make it brighter (intended for 32-bit mode)
	uint bIsEngineGlow;			// 1 if this is an engine glow textures, 2 will make it brighter (intended for 32-bit mode)
	uint bIsHyperspaceStreak;	// 1 if we're rendering hyperspace streaks
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

PixelShaderOutput main(PixelShaderInput input)
{
	PixelShaderOutput output;
	float4 texelColor = texture0.Sample(sampler0, input.tex);
	float alpha = texelColor.w;
	float3 diffuse = input.color.xyz;
	// Zero-out the bloom mask.
	output.bloom = float4(0, 0, 0, 0);
	//output.bloom = float4(0.2 * texelColor.xyz, alpha * 0.2);
	output.color = texelColor;

	// Process lasers (make them brighter in 32-bit mode)
	if (bIsLaser) {
		// This is a laser texture, process the bloom mask accordingly
		float3 HSV = RGBtoHSV(texelColor.xyz);
		if (bIsLaser > 1) {
			// Enhance the lasers in 32-bit mode
			// Increase the saturation and lightness
			HSV.y *= 1.5;
			HSV.z *= 2.0;
			float3 color = HSVtoRGB(HSV);
			output.color = float4(color, 1.2 * alpha);
			// Enhance the saturation even more for the bloom laser mask
			HSV.y *= 4.0;
			color = HSVtoRGB(HSV);
			output.bloom = float4(color, alpha);
		}
		else {
			output.color = texelColor; // Return the original color when 32-bit mode is off
			// Enhance the saturation for lasers
			HSV.y *= 4.0;
			float3 color = HSVtoRGB(HSV);
			output.bloom = float4(color, alpha);
		}
		return output;
	}

	// Process light textures (make them brighter in 32-bit mode)
	if (bIsLightTexture) {
		// This is a light texture, process the bloom mask accordingly
		float3 HSV = RGBtoHSV(texelColor.xyz);
		float val = HSV.z;
		if (bIsLightTexture > 1) {
			// Make the light textures brighter in 32-bit mode
			HSV.z *= 1.25;
			// The alpha for light textures is either 0 or >0.1, so we multiply by 10 to
			// make it [0, 1]
			alpha *= 10.0;
			float3 color = HSVtoRGB(HSV);
			if (val > 0.8 && alpha > 0.5)
				output.bloom = float4(val * color, 1);
			output.color = float4(color, alpha);
		}
		else {
			if (val > 0.8 && alpha > 0.5)
				output.bloom = float4(val * texelColor.rgb, 1);
			output.color = texelColor;	// Return the original color when 32-bit mode is off
		}
		return output;
	}

	// Enhance engine glow. In this texture, the diffuse component also provides
	// the hue
	if (bIsEngineGlow) {
		//return float4(0, 1, 0, alpha);
		texelColor.xyz *= diffuse;
		// This is an engine glow, process the bloom mask accordingly
		if (bIsEngineGlow > 1) {
			// Enhance the glow in 32-bit mode
			float3 HSV = RGBtoHSV(texelColor.xyz);
			HSV.y *= 1.15;
			HSV.z *= 1.25;
			float3 color = HSVtoRGB(HSV);
			output.color = float4(color, alpha);
			output.bloom = float4(color, alpha);
		}
		else {
			output.color = texelColor; // Return the original color when 32-bit mode is off
			output.bloom = float4(texelColor.rgb, alpha);
		}
		return output;
	}
	
	if (bIsHyperspaceAnim)
		output.bloom = float4(texelColor.xyz, 0.5);

	if (bIsHyperspaceStreak)
		output.bloom = float4(0.7, 0.7, 1, 0.7);

	output.color = float4(brightness * diffuse * texelColor.xyz, texelColor.w);
	return output;
}
