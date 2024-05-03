// Copyright (c) 2021 Leo Reyes
// Licensed under the MIT license. See LICENSE.txt
#include "XwaD3dCommon.hlsl"
#include "shader_common.h"
#include "HSV.h"
#include "shading_system.h"
#include "PixelShaderTextureCommon.h"
#include "NormalMapping.h"

Texture2D	 texture0 : register(t0);
Texture2D	 texture1 : register(t1); // If present, this is the light texture
SamplerState sampler0 : register(s0);
// Normal Map, slot 13
Texture2D	 normalMap : register(t13);

// Texture slot 9 (and above) seem to be free. We might be able to use other slots, but I don't
// want to break something by doing that. I'll check that later

// Normal Map, slot 9
Texture2D    normalMapTex : register(t9);
SamplerState normalMapSamp : register(s9);

// Greeble Texture 1, slot 10
Texture2D    greebleTex1 : register(t10);
SamplerState greebleSamp1 : register(s10);

// Greeble Texture 2, slot 11
Texture2D    greebleTex2 : register(t11);
SamplerState greebleSamp2 : register(s11);

// Greeble Texture 3, slot 12
Texture2D    greebleTex3 : register(t12);
SamplerState greebleSamp3 : register(s12);

// pos3D/Depth buffer has the following coords:
// X+: Right
// Y+: Up
// Z+: Away from the camera
// (0,0,0) is the camera center, (0,0,Z) is the center of the screen
// The Normal buffer is different:
// Z+ points towards the camera.

struct PixelShaderInput
{
	float4 pos    : SV_POSITION;
	float4 pos3D  : COLOR1;
	float4 normal : NORMAL;
	float2 tex	  : TEXCOORD;
	//float4 color  : COLOR0;
	//float4 tangent : TANGENT;
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

inline float4 overlay(float4 a, float4 b)
{
	return (a < 0.5) ? 2.0 * a * b : 1.0 - 2.0 * (1.0 - a) * (1.0 - b);
}

inline float4 screen(float4 a, float4 b)
{
	return 1.0 - (1.0 - a) * (1.0 - b);
}

// Apply greebles to color and/or normal.
// color can be either a regular texture or a lightmap texture.
void Greeble(inout float4 color, inout float4 normal, in float2 tex, in float3 P, in float3x3 TBN)
{
	/*
	uint GreebleControl;
		Bitmask:
			0x1000 -- Use Normal Map
			0x2000 -- This is Lightmap Greeble
		Masks:
			0x00F First Tex Blend Mode
			0x0F0 Second Tex Blend Mode
			0xF00 Third Tex Blend Mode (not implemented yet)
		Blend Modes:
			GBM_MULTIPLY = 1,
			GBM_OVERLAY = 2,
			GBM_SCREEN = 3,
			GBM_REPLACE = 4,
			GBM_NORMAL_MAP = 5,
			GBM_UV_DISP = 6,
			GBM_UV_DISP_AND_NORMAL_MAP = 7
	*/

	uint BlendingMode1 = GreebleControl & 0xF;
	uint BlendingMode2 = (GreebleControl >> 4) & 0xF;
	//uint BlendingMode3 = (GreebleControl >> 8) & 0xF;
	//bool bUsesNormalMap = (GreebleControl & 0x10000) != 0x0;
	bool bIsLightmapGreeble = (GreebleControl & 0x20000) != 0x0;
	//float3x3 TBN;

	// Compute the TBN matrix if any of our blending modes require normal mapping
	//if (bUsesNormalMap)
	//	TBN = cotangent_frame(normal.xyz, P, tex);

	float4 greeble1 = 0.0, greeble2 = 0.0;
	float4 greebleMixCol = 0.0;
	const float2 distBlendFactors = saturate(P.zz / float2(GreebleDist1, GreebleDist2));

	if (BlendingMode1 != 0 && distBlendFactors.x <= 1.0) {
		greeble1 = greebleTex1.Sample(greebleSamp1, frac(GreebleScale1 * tex));
		// Convert grayscale into transparency for lightmaps
		if (bIsLightmapGreeble) greeble1.a *= dot(0.333, greeble1.rgb);

		if (BlendingMode1 == 1)
			greebleMixCol = lerp(color, color * greeble1, GreebleMix1 * greeble1.a);
		else if (BlendingMode1 == 2)
			greebleMixCol = lerp(color, overlay(color, greeble1), GreebleMix1 * greeble1.a);
		else if (BlendingMode1 == 3)
			greebleMixCol = lerp(color, screen(color, greeble1), GreebleMix1 * greeble1.a);
		else if (BlendingMode1 == 4)
			// For the replace mode, we ignore greeble1.a because we want to replace the original texture everywhere,
			// including the transparent areas
			greebleMixCol = lerp(color, greeble1, GreebleMix1);
		else if (BlendingMode1 == 5) {
			normal.xyz = lerp(normal.xyz,
				//perturb_normal(normal.xyz, /* g_viewvector */ -P, tex, greeble1.rgb),
				perturb_normal(TBN, normal.xyz, greeble1.rgb),
				GreebleMix1 * greeble1.a);
			greebleMixCol = color;
		}
		else if (BlendingMode1 == 6) {
			// UV displacement
			float2 UVDisp = ((greeble1.xy - 0.5) * 2.0) / UVDispMapResolution;
			greebleMixCol = texture0.Sample(sampler0, frac(tex + UVDisp * GreebleMix1 * greeble1.a));
		}
		else if (BlendingMode1 == 7) {
			// UV displacement
			float2 UVDisp = ((greeble1.xy - 0.5) * 2.0) / UVDispMapResolution;
			greebleMixCol = texture0.Sample(sampler0, frac(tex + UVDisp * (2.0*GreebleMix1) * greeble1.a));
			// Normal Mapping
			normal.xyz = lerp(normal.xyz,
				//perturb_normal(normal.xyz, /* g_viewvector */ -P, tex, greeble1.rgb),
				perturb_normal(TBN, normal.xyz, greeble1.rgb),
				GreebleMix1 * greeble1.a);
		}
		// Mix the greeble color depending on the current depth
		color = lerp(greebleMixCol, color, distBlendFactors.x);
	}

	if (BlendingMode2 != 0 && distBlendFactors.y <= 1.0) {
		greeble2 = greebleTex2.Sample(greebleSamp2, frac(GreebleScale2 * tex));
		// Convert grayscale into transparency for lightmaps
		if (bIsLightmapGreeble) greeble2.a *= dot(0.333, greeble2.rgb);

		if (BlendingMode2 == 1)
			greebleMixCol = lerp(color, color * greeble2, GreebleMix2 * greeble2.a);
		else if (BlendingMode2 == 2)
			greebleMixCol = lerp(color, overlay(color, greeble2), GreebleMix2 * greeble2.a);
		else if (BlendingMode2 == 3)
			greebleMixCol = lerp(color, screen(color, greeble2), GreebleMix2 * greeble2.a);
		else if (BlendingMode2 == 4)
			// For the replace mode, we ignore greeble2.a because we want to replace the original texture everywhere,
			// including the transparent areas
			greebleMixCol = lerp(color, greeble2, GreebleMix2);
		else if (BlendingMode2 == 5) {
			normal.xyz = lerp(normal.xyz,
				//perturb_normal(normal.xyz, /* g_viewvector */ -P, tex, greeble2.rgb),
				perturb_normal(TBN, normal.xyz, greeble2.rgb),
				GreebleMix2 * greeble2.a);
			greebleMixCol = color;
		}
		else if (BlendingMode1 == 6) {
			// UV displacement
			float2 UVDisp = ((greeble2.xy - 0.5) * 2.0) / UVDispMapResolution;
			greebleMixCol = texture0.Sample(sampler0, frac(tex + UVDisp * GreebleMix2 * greeble2.a));
		}
		else if (BlendingMode1 == 7) {
			// UV displacement
			float2 UVDisp = ((greeble1.xy - 0.5) * 2.0) / UVDispMapResolution;
			greebleMixCol = texture0.Sample(sampler0, frac(tex + UVDisp * (2.0*GreebleMix2) * greeble2.a));
			// Normal Mapping
			normal.xyz = lerp(normal.xyz,
				//perturb_normal(normal.xyz, /* g_viewvector */ -P, tex, greeble2.rgb),
				perturb_normal(TBN, normal.xyz, greeble2.rgb),
				GreebleMix2 * greeble2.a);
		}
		// Mix the greeble color depending on the current depth
		color = lerp(greebleMixCol, color, distBlendFactors.y);
	}
}

PixelShaderOutput main(PixelShaderInput input)
{
	PixelShaderOutput output;
	float4 texelColor     = texture0.Sample(sampler0, input.tex);
	float3 normalMapColor = bDoNormalMapping ? normalMap.Sample(sampler0, input.tex).rgb : float3(0, 0, 1);
	float  alpha          = texelColor.w;
	float3 P              = input.pos3D.xyz;
	float  SSAOAlpha      = saturate(min(alpha - fSSAOAlphaOfs, fPosNormalAlpha));
	// Zero-out the bloom mask.
	output.bloom = 0;
	output.color = texelColor;
	output.pos3D = float4(P, SSAOAlpha);
	output.ssMask = 0;

	// DEBUG
	//output.normal = 0;
	//output.ssaoMask = 0;
	//output.color = float4(brightness * diffuse * texelColor.xyz, texelColor.w);
	//return output;
	// DEBUG

	// DEBUG
		//output.color = float4(frac(input.tex.xy), 0, 1); // DEBUG: Display the uvs as colors
		//output.ssaoMask = float4(SHADELESS_MAT, 0, 0, 1);
		//output.ssMask = 0;
		//output.normal = 0;
		//return output;
	// DEBUG

	float3 N = normalize(input.normal.xyz);
	N.yz = -N.yz; // Invert the Y axis, originally Y+ is down

	const float3x3 TBN = cotangent_frame(N, P, input.tex);

	// Replicate the same transforms we're applying to the normal N
	//float3 T = normalize(input.tangent.xyz);
	//T.yz = -T.yz;
	//const float3 B = cross(T, N);
	//const float3x3 TBN = float3x3(T, B, N);
	if (bDoNormalMapping) {
		const float3 NM = normalize(mul((normalMapColor * 2.0) - 1.0, TBN));
		N = lerp(N, NM, fNMIntensity);
	}
	output.normal = float4(N, SSAOAlpha);

	// ssaoMask: Material, Glossiness, Specular Intensity
	output.ssaoMask = float4(fSSAOMaskVal, fGlossiness, fSpecInt, alpha);
	// SS Mask: unused (Normal Mapping Intensity), Specular Value, Shadeless
	output.ssMask = float4(0, fSpecVal, fAmbient, alpha);

	// We shouldn't call this shader for smoke, lasers or engine glow
	
	// Never call this shader for the following:
	// The HUD is shadeless and has transparency. Some planets in the background are also 
	// transparent (CHECK IF Jeremy's latest hooks fixed this) 
	// So glass is a non-shadeless surface with transparency:
	//if (fSSAOMaskVal < SHADELESS_LO && !bIsShadeless && alpha < 0.95) {
	
	// Original code:
	output.color = float4(brightness * texelColor.xyz, texelColor.w);

	bool bIsLightmapGreeble = (GreebleControl & 0x20000) != 0x0;

	float4 texelColorIllum = texture1.Sample(sampler0, input.tex);
	float4 GreebleColor = bIsLightmapGreeble ? texelColorIllum : output.color;
	Greeble(GreebleColor, output.normal, input.tex, P, TBN);
	if (bIsLightmapGreeble)
		texelColorIllum = GreebleColor;
	else
		output.color = GreebleColor;

	// In the D3dRendererHook, lightmaps and regular textures are rendered on the same draw call.
	// Here's the case where a lightmap has been provided:
	if (renderTypeIllum == 1)
	{
		// We have a lightmap texture
		// We don't sample texelColorIllum here, it's sampled (and greebled) above.		
		float3 color = texelColorIllum.rgb;
		float alpha = texelColorIllum.a;
		// This is a light texture, process the bloom mask accordingly
		float3 HSV = RGBtoHSV(color);
		float val = HSV.z;
		// Make the light textures brighter in 32-bit mode
		HSV.z *= 1.25;
		// The alpha for light textures is either 0 or >0.1, so we multiply by 10 to make it [0, 1]
		alpha *= 10.0;
		color = HSVtoRGB(HSV);

		const float bloom_alpha = smoothstep(0.75, 0.85, val) * smoothstep(0.45, 0.55, alpha);
		output.bloom = float4(bloom_alpha * val * color, bloom_alpha);
		// Write an emissive material where there's bloom:
		output.ssaoMask.r = lerp(output.ssaoMask.r, bloom_alpha, bloom_alpha);
		// Set fNMIntensity to 0 where we have bloom:
		output.ssMask.r = lerp(output.ssMask.r, 0, bloom_alpha);
		// Replace the current color with the lightmap color, where appropriate:
		output.color.rgb = lerp(output.color.rgb, color, bloom_alpha);
		// Apply the bloom strength to this lightmap
		output.bloom.rgb *= fBloomStrength;
		if (bInHyperspace && output.bloom.a < 0.5)
			discard;
	}

	return output;
}
