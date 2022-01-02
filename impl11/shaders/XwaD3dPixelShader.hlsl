#include "XwaD3dCommon.hlsl"
#include "shader_common.h"
#include "HSV.h"
#include "PixelShaderTextureCommon.h"

Texture2D texture0 : register(t0); // This is the regular color texture
Texture2D texture1 : register(t1); // If present, this is the light texture
SamplerState sampler0 : register(s0);

// Texture slot 9 (and above) seem to be free. We might be able to use other slots, but I don't
// want to break something by doing that. Will have to come back later and check if it's possible
// to save some slots
Texture2D    greebleTex0 : register(t9);
SamplerState greebleSamp0 : register(s9);

/*
// This is the original PixelShaderInput used in the D3dRendererHook
struct PixelShaderInput
{
	float4 pos : SV_POSITION;
	float4 position : COLOR2;
	float4 normal : COLOR1;
	float2 tex : TEXCOORD;
	float4 color : COLOR0;
};

// This is the original Pixel shader used in the D3dRendererHook
float4 main(PixelShaderInput input) : SV_TARGET
{
	//if (input.color.w == 1.0f)
	//{
	//	return input.color;
	//}

	float4 color = float4(input.color.xyz, 1.0f);
	float4 texelColor = texture0.Sample(sampler0, input.tex);

	float4 c = texelColor * color;

	if (renderTypeIllum == 1)
	{
		float4 texelColorIllum = texture1.Sample(sampler0, input.tex);
		c.xyz += texelColorIllum.xyz * texelColorIllum.w;
		c = saturate(c);
	}

	return c;
}
*/

struct PixelShaderInput
{
	float4 pos    : SV_POSITION;
	float4 pos3D  : COLOR2;
	float4 normal : COLOR1;
	float2 tex	  : TEXCOORD;
	//float4 color  : COLOR0;
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
	// This is the per-vertex Gouraud-shaded color coming from the VR:
	//float4 color			= float4(input.color.xyz, 1.0f);
	float4 texelColor		= texture0.Sample(sampler0, input.tex);
	uint   bIsBlastMark		= special_control & SPECIAL_CONTROL_BLAST_MARK;
	if (bIsBlastMark)
		texelColor			= texture0.Sample(sampler0, (input.tex * 0.35) + 0.3);

	float  alpha				= texelColor.w;
	//float3 diffuse			= lerp(input.color.xyz, 1.0, fDisableDiffuse);
	float3 P					= input.pos3D.xyz;
	float  SSAOAlpha			= saturate(min(alpha - fSSAOAlphaOfs, fPosNormalAlpha));
	uint   ExclusiveMask		= special_control & SPECIAL_CONTROL_EXCLUSIVE_MASK;
	// Zero-out the bloom mask and provide default output values
	output.bloom		= 0;
	output.color		= texelColor;
	output.pos3D		= float4(P, SSAOAlpha);
	output.ssMask	= 0;

	// hook_normals code:
	float3 N = normalize(input.normal.xyz * 2.0 - 1.0);
	N.y = -N.y; // Invert the Y axis, originally Y+ is down
	N.z = -N.z;
	output.normal = float4(N, SSAOAlpha);

	// SSAO Mask/Material, Glossiness, Spec_Intensity
	// Glossiness is multiplied by 128 to compute the exponent
	//output.ssaoMask = float4(fSSAOMaskVal, DEFAULT_GLOSSINESS, DEFAULT_SPEC_INT, alpha);
	// ssaoMask.r: Material
	// ssaoMask.g: Glossiness
	// ssaoMask.b: Specular Intensity
	output.ssaoMask = float4(fSSAOMaskVal, fGlossiness, fSpecInt, alpha);
	// SS Mask: Normal Mapping Intensity, Specular Value, Shadeless
	output.ssMask = float4(fNMIntensity, fSpecVal, fAmbient, alpha);

	//float4 c = texelColor * color;

	// In the D3dRendererHook, lightmaps and regular textures are rendered on the same draw call.
	// Here's the case where a lightmap has been provided:
	if (renderTypeIllum == 1)
	{
		// We have a lightmap texture
		float4 texelColorIllum = texture1.Sample(sampler0, input.tex);
		
		//c.xyz += texelColorIllum.xyz * texelColorIllum.w;
		//c = saturate(c);

		float3 color = texelColorIllum.rgb;
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
