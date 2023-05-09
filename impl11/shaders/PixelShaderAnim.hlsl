// Copyright (c) 2021 Leo Reyes
// This shader is used to display both animated textures & light maps.
// Transparent areas won't change the previous contents; but this shader
// can be used to render solid areas just like the regular pixel shader.
// The main difference with the regular light map shader is that the alpha
// of the light map is not multiplied by 10.
// This shader also has code from the old version of PixelShaderNoGlass.
// Licensed under the MIT license. See LICENSE.txt
#include "XwaD3dCommon.hlsl"
#include "shader_common.h"
#include "HSV.h"
#include "shading_system.h"
#include "PixelShaderTextureCommon.h"

Texture2D    texture0 : register(t0);
Texture2D	 texture1 : register(t1); // If present, this is the light texture
SamplerState sampler0 : register(s0);
// Normal Map, slot 13
Texture2D   normalMap : register(t13);
// Universal Hull Damage texture, slot 14
Texture2D overlayTexA : register(t14);
// Universal Shields Down texture, slot 15
Texture2D overlayTexB : register(t15);

struct PixelShaderInput
{
	float4 pos    : SV_POSITION;
	float4 pos3D  : COLOR1;
	float4 normal : NORMAL;
	float2 tex	  : TEXCOORD;
	//float4 color  : COLOR0;
	float4 tangent : TANGENT;
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

	const uint bIsBlastMark       = special_control & SPECIAL_CONTROL_BLAST_MARK;
	const uint ExclusiveMask      = special_control & SPECIAL_CONTROL_EXCLUSIVE_MASK;
	const uint ExclusiveMaskLight = special_control_light & SPECIAL_CONTROL_EXCLUSIVE_MASK;

	float2 UV = input.tex * float2(AspectRatio, 1) + Offset.xy;
	if (Clamp) UV = saturate(UV);
	float specInt = fSpecInt;
	float glossiness = fGlossiness;
	float4 ScreenLyrBloom = 0;

	float4 texelColor = AuxColor * texture0.Sample(sampler0, UV);
	if (bIsBlastMark)
		texelColor = texture0.Sample(sampler0, (input.tex * 0.35) + 0.3);
	//if (ExclusiveMask == SPECIAL_CONTROL_GRAYSCALE)
	//	texelColor.rgb = float3(0.7, 0.7, 0.7);

	const bool uvActive = uvSrc0.x <= input.tex.x && input.tex.x <= uvSrc1.x &&
			              uvSrc0.y <= input.tex.y && input.tex.y <= uvSrc1.y;
	const float2 uvRange = uvSrc1 - uvSrc0;

	// Apply the damage texture
	if ((OverlayCtrl & OVERLAY_CTRL_MULT) != 0)
	{
		const float2 uv = frac((input.tex - uvSrc0) / uvRange);
		const float4 multColor = uvActive ? overlayTexA.Sample(sampler0, uv) : 1.0;
		texelColor.rgb *= multColor.rgb;
		specInt *= multColor.r;
		glossiness *= multColor.r;
	}
	
	if ((OverlayCtrl & OVERLAY_CTRL_SCREEN) != 0)
	{
		// rand is a random value provided by the CPU in the range 0..1
		// It is used here to compute a rotation matrix that rotates UVs
		// in the same range (0..180 degrees).
		float s, c, ang = rand0;
		sincos(ang, s, c);
		float2x2 mat = float2x2(c, s, -s, c);
		float2 uv = frac(mul(input.tex.xy, mat) + float2(rand1, rand2));
		// The screen layer will need its own set of uvSrc coords and uvActive.
		//const float4 layerColor = uvActive ? overlayTexB.Sample(sampler0, uv) : 0.0;
		const float4 layerColor = overlayTexB.Sample(sampler0, uv);
		texelColor.rgb = 1.0 - ((1.0 - texelColor.rgb) * (1.0 - layerColor.rgb));
		const float val = dot(float3(0.33, 0.5, 0.16), layerColor.rgb) * layerColor.a;
		ScreenLyrBloom = fOverlayBloomPower * val * layerColor;
	}

	float  alpha = texelColor.a;
	float3 HSV = RGBtoHSV(texelColor.rgb);

	if (ExclusiveMask == SPECIAL_CONTROL_BLACK_TO_ALPHA)
		alpha = HSV.z;

	float4 texelColorLight = AuxColorLight * texture1.Sample(sampler0, UV);
	float  alphaLight = texelColorLight.a;
	float3 colorLight = texelColorLight.rgb;
	float3 HSVLight = RGBtoHSV(colorLight);
	if (ExclusiveMaskLight == SPECIAL_CONTROL_BLACK_TO_ALPHA)
		alphaLight = HSVLight.z;

	float3 P = input.pos3D.xyz;
	float  SSAOAlpha = saturate(min(alpha - fSSAOAlphaOfs, fPosNormalAlpha));
	// Zero-out the bloom mask and provide default output values
	output.bloom = 0;
	output.color = texelColor;
	output.pos3D = float4(P, SSAOAlpha);
	output.ssMask = 0;

	float3 N = normalize(input.normal.xyz);
	N.y = -N.y; // Invert the Y axis, originally Y+ is down
	N.z = -N.z;

	if (bDoNormalMapping) {
		float3 normalMapColor = normalMap.Sample(sampler0, input.tex).rgb;
		float3 T = normalize(input.tangent.xyz);
		T.y = -T.y; // Replicate the same transforms we're applying to the normal N
		T.z = -T.z;
		const float3 B = cross(T, N);
		const float3x3 TBN = float3x3(T, B, N);
		const float3 NM = normalize(mul((normalMapColor * 2.0) - 1.0, TBN));
		N = lerp(N, NM, fNMIntensity);
	}
	output.normal = float4(N, SSAOAlpha);

	// ssaoMask: Material, Glossiness, Specular Intensity
	// I don't like this "fix" but I don't have a better solution at the moment.
	// Sometimes, displaying broken glass will look weird. The reason seems to be related
	// to the material type. When glass is rendered, GLASS_MAT is set in ssaoMask.r, but
	// that will get blended with whatever material is already there. Sometimes this causes
	// the material to change. To prevent that, I'm dividing by alpha to ensure that the
	// material stays shadeless. This isn't a proper fix. Instead, we should get rid of
	// the multiple materials in channel ssaoMask.r and instead just keep separate channels
	// for metallicity, glossiness, etc. This should work for now, though.
	output.ssaoMask = float4(fSSAOMaskVal/alpha, glossiness, specInt, alpha);
	// SS Mask: unused, Specular Value, Shadeless
	output.ssMask = float4(0, fSpecVal, fAmbient, alpha);

	// The regular layer might have transparency. If we're in hyperspace, we don't want to show
	// the background through it, so we mix it with a black color
	// Update: looks like we don't need to do this anymore (?) In fact, enabling this line will
	// cause the cockpit glass to go black if it's damaged before jumping into hyperspace
	//if (bInHyperspace) output.color = float4(lerp(float3(0,0,0), output.color.rgb, alpha), 1);

	if (renderTypeIllum == 1)
	{
		// This is a light texture, process the bloom mask accordingly
		float val = HSVLight.z;
		HSVLight.z *= 1.25;
		//alpha *= 10.0; <-- Main difference with XwaD3dPixelShader
		colorLight = saturate(HSVtoRGB(HSVLight)); // <-- Difference wrt XwaD3dPixelShader
		
		float bloom_alpha = smoothstep(0.75, 0.85, val) * smoothstep(0.45, 0.55, alphaLight);
		output.bloom = float4(bloom_alpha * val * colorLight, bloom_alpha);
		// Write an emissive material where there's bloom:
		output.ssaoMask.r = lerp(output.ssaoMask.r, bloom_alpha, bloom_alpha);
		// Set fNMIntensity to 0 where we have bloom:
		output.ssMask.r = lerp(output.ssMask.r, 0, bloom_alpha);
		// Replace the current color with the lightmap color, where appropriate:
		output.color.rgb = lerp(output.color.rgb, colorLight, alphaLight);
		//output.color.a = max(output.color.a, alphaLight);
		// Apply the bloom strength to this lightmap
		output.bloom.rgb *= fBloomStrength;
		if (ExclusiveMaskLight == SPECIAL_CONTROL_ALPHA_IS_BLOOM_MASK) {
			output.bloom = lerp(output.bloom, float4(0, 0, 0, 1), alphaLight);
			output.ssaoMask = lerp(float4(0, 0, 0, 1), float4(output.ssaoMask.rgb, alphaLight), alphaLight);
			output.ssMask = lerp(float4(0, 0, 0, 1), float4(output.ssMask.rgb, alphaLight), alphaLight);
		}
		// The following line needs to be updated, it was originally
		// intended to remove the lightmap layer, but keep the regular
		// layer. Doing discard now will remove both layers!
		//if (bInHyperspace && output.bloom.a < 0.5)
		//	discard;
	}
	output.bloom = max(output.bloom, ScreenLyrBloom);

	return output;
}