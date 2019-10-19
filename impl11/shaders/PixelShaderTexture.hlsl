// Copyright (c) 2014 Jérémy Ansel
// Licensed under the MIT license. See LICENSE.txt
// Extended for VR by Leo Reyes (c) 2019
#include "HSV.h"

Texture2D    texture0 : register(t0);
SamplerState sampler0 : register(s0);

static float METRIC_SCALE_FACTOR = 25.0;

struct PixelShaderInput
{
	float4 pos    : SV_POSITION;
	float4 color  : COLOR0;
	float2 tex    : TEXCOORD0;
	float4 pos3D  : COLOR1;
};

struct PixelShaderOutput
{
	float4 color    : SV_TARGET0;
	float4 diffuse  : SV_TARGET1;
	float4 bloom    : SV_TARGET2;
	float4 pos3D    : SV_TARGET3;
	float4 normal   : SV_TARGET4;
	float4 ssaoMask : SV_TARGET5;
};

cbuffer ConstantBuffer : register(b0)
{
	float brightness;			// Used to dim some elements to prevent the Bloom effect -- mostly for ReShade compatibility
	uint DynCockpitSlots;		// (Unused here) How many DC slots will be used.
	uint bUseCoverTexture;		// (Unused here) When set, use the first texture as cover texture for the dynamic cockpit
	uint bIsHyperspaceAnim;		// 1 if we're rendering the hyperspace animation
	// 16 bytes

	uint bIsLaser;				// 1 for Laser objects, setting this to 2 will make them brighter (intended for 32-bit mode)
	uint bIsLightTexture;		// 1 if this is a light texture, 2 will make it brighter (intended for 32-bit mode)
	uint bIsEngineGlow;			// 1 if this is an engine glow textures, 2 will make it brighter (intended for 32-bit mode)
	uint bIsHyperspaceStreak;	// 1 if we're rendering hyperspace streaks
	// 32 bytes

	float fBloomStrength;		// General multiplier for the bloom effect
	float fPosNormalAlpha;		// Override for pos3D and normal output alpha
	float fSSAOMaskVal;			// SSAO mask value
	float unused;
	// 48 bytes
};

PixelShaderOutput main(PixelShaderInput input)
{
	PixelShaderOutput output;
	float4 texelColor = texture0.Sample(sampler0, input.tex);
	float alpha = texelColor.w;
	float3 diffuse = input.color.xyz;
	// Zero-out the bloom mask.
	output.bloom = 0;
	output.color = texelColor;

	//float3 light = normalize(float3(1, 1, -0.5));
	//light = normalize(mul(viewMatrix, float4(light, 0)).xyz);

	float3 P = float3(input.pos3D.xyz);
	output.pos3D  = float4(P, fPosNormalAlpha);
	
	float3 N = normalize(cross(ddx(P), ddy(P)));
	//N = normalize(lerp(N, light, diffuse.x));
	output.normal = float4(N, fPosNormalAlpha);
	
	output.ssaoMask = float4(fSSAOMaskVal, fSSAOMaskVal, fSSAOMaskVal, alpha);
	output.diffuse = input.color;

	// Process lasers (make them brighter in 32-bit mode)
	if (bIsLaser) {
		output.pos3D.a = 0;
		output.normal.a = 0;
		output.ssaoMask.a = 0;
		output.diffuse = 0;
		// This is a laser texture, process the bloom mask accordingly
		float3 HSV = RGBtoHSV(texelColor.xyz);
		if (bIsLaser > 1) {
			// Enhance the lasers in 32-bit mode
			// Increase the saturation and lightness
			HSV.y *= 1.5;
			HSV.z *= 2.0;
			float3 color = HSVtoRGB(HSV);
			output.color = float4(color, alpha);
			// Enhance the saturation even more for the bloom laser mask
			//HSV.y *= 4.0;
			//color = HSVtoRGB(HSV);
			output.bloom = float4(color, alpha);
		}
		else {
			output.color = texelColor; // Return the original color when 32-bit mode is off
			// Enhance the saturation for lasers
			HSV.y *= 1.5;
			float3 color = HSVtoRGB(HSV);
			output.bloom = float4(color, alpha);
		}
		output.bloom.rgb *= fBloomStrength;
		return output;
	}

	// Process light textures (make them brighter in 32-bit mode)
	if (bIsLightTexture) {
		//output.pos3D = 0;
		//output.normal = 0;
		//output.diffuse = 0;
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
			if (val > 0.8 && alpha > 0.5) {
				output.bloom = float4(val * color, 1);
				output.ssaoMask = 1;
			}
			output.color = float4(color, alpha);
		}
		else {
			if (val > 0.8 && alpha > 0.5) {
				output.bloom = float4(val * texelColor.rgb, 1);
				output.ssaoMask = 1;
			}
			output.color = texelColor;	// Return the original color when 32-bit mode is off
		}
		output.bloom.rgb *= fBloomStrength;
		return output;
	}

	// Enhance the engine glow. In this texture, the diffuse component also provides
	// the hue
	if (bIsEngineGlow) {
		// Disable depth-buffer write for engine glow textures
		output.pos3D.a = 0;
		output.normal.a = 0;
		output.diffuse = 0;
		texelColor.xyz *= diffuse;
		// This is an engine glow, process the bloom mask accordingly
		if (bIsEngineGlow > 1) {
			// Enhance the glow in 32-bit mode
			float3 HSV = RGBtoHSV(texelColor.xyz);
			//HSV.y *= 1.15;
			HSV.y *= 1.25;
			HSV.z *= 1.25;
			float3 color = HSVtoRGB(HSV);
			output.color = float4(color, alpha);
		} 
		else
			output.color = texelColor; // Return the original color when 32-bit mode is off
		output.bloom = float4(fBloomStrength * output.color.rgb, alpha);
		return output;
	}

	if (bIsHyperspaceAnim) {
		output.pos3D.a = 0;
		output.normal.a = 0;
		//output.diffuse = 0;
		output.bloom = float4(fBloomStrength * texelColor.xyz, 0.5);
	}

	if (bIsHyperspaceStreak) {
		output.pos3D.a = 0;
		output.normal.a = 0;
		//output.diffuse = 0;
		output.bloom = float4(fBloomStrength * float3(0.5, 0.5, 1), 0.5);
	}

	output.color = float4(brightness * diffuse * texelColor.xyz, texelColor.w);
	//output.color = float4(texelColor.xyz, texelColor.w); // No diffuse component
	return output;
}
