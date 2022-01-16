#include "XwaD3dCommon.hlsl"
#include "HSV.h"
#include "shader_common.h"
#include "shading_system.h"
#include "PixelShaderTextureCommon.h"

Texture2D texture0 : register(t0); // This is the regular color texture
Texture2D texture1 : register(t1); // If present, this is the light texture
SamplerState sampler0 : register(s0);

//#define ORIGINAL_D3D_RENDERER_SHADERS
#ifdef ORIGINAL_D3D_RENDERER_SHADERS
// This is the original PixelShaderInput used in the D3dRendererHook
struct PixelShaderInput
{
	float4 pos		: SV_POSITION;
	float4 position : COLOR2;
	float4 normal	: COLOR1;
	float2 tex		: TEXCOORD;
	float4 color		: COLOR0;
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

	// DEBUG: Display normal data
	c.rgb = (normalize(input.normal.xyz) + 1.0) * 0.5;
	return c;
}
#else
struct PixelShaderInput
{
	float4 pos    : SV_POSITION;
	float4 pos3D  : COLOR1;
	float4 normal : NORMAL;
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
	uint bIsBlastMark		= special_control & SPECIAL_CONTROL_BLAST_MARK;
	uint ExclusiveMask		= special_control & SPECIAL_CONTROL_EXCLUSIVE_MASK;
	if (bIsBlastMark)
		texelColor			= texture0.Sample(sampler0, (input.tex * 0.35) + 0.3);

	float  alpha				= texelColor.w;
	float3 P					= input.pos3D.xyz;
	float  SSAOAlpha			= saturate(min(alpha - fSSAOAlphaOfs, fPosNormalAlpha));
	
	// Zero-out the bloom mask and provide default output values
	output.bloom		= 0;
	output.color		= output.color = float4(brightness * texelColor.xyz, texelColor.w);
	output.pos3D		= float4(P, SSAOAlpha);
	output.ssMask	= 0;

	float3 N = normalize(input.normal.xyz);
	N.y = -N.y; // Invert the Y axis, originally Y+ is down
	N.z = -N.z;
	output.normal = float4(N, SSAOAlpha);

	// ssaoMask: Material, Glossiness, Specular Intensity
	output.ssaoMask = float4(fSSAOMaskVal, fGlossiness, fSpecInt, alpha);
	// SS Mask: Normal Mapping Intensity, Specular Value, Shadeless
	output.ssMask = float4(fNMIntensity, fSpecVal, fAmbient, alpha);

	// Process lasers
	if (renderType == 2)
	{
		// Do not write the 3D position
		output.pos3D.a = 0;
		// Do now write the normal
		output.normal.a = 0;
		//output.ssaoMask.a = 1; // Needed to write the emission material for lasers
		output.ssaoMask.a = 0; // We should let the regular material properties on lasers so that they become emitters
		output.ssMask.a = 0;
		//output.diffuse = 0;
		// This is a laser texture, process the bloom mask accordingly
		float3 HSV = RGBtoHSV(texelColor.xyz);
		HSV.y *= 1.5;
		// Enhance the lasers in 32-bit mode
		// Increase the saturation and lightness
		HSV.z *= 2.0;
		float3 color = HSVtoRGB(HSV);
		output.color = float4(color, alpha);
		output.bloom = float4(color, alpha);
		output.bloom.rgb *= fBloomStrength;
		return output;
	}

	// In the D3dRendererHook, lightmaps and regular textures are rendered on the same draw call.
	// Here's the case where a lightmap has been provided:
	if (renderTypeIllum == 1)
	{
		// We have a lightmap texture
		float4 texelColorIllum = texture1.Sample(sampler0, input.tex);
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
		return output;
	}

	// The HUD is shadeless and has transparency. Some planets in the background are also 
	// transparent (CHECK IF Jeremy's latest hooks fixed this) 
	// So glass is a non-shadeless surface with transparency:
	if (fSSAOMaskVal < SHADELESS_LO /* This texture is *not* shadeless */
		&& !bIsShadeless /* Another way of saying "this texture isn't shadeless" */
		&& alpha < 0.95 /* This texture has transparency */
		&& !bIsBlastMark) /* Blast marks have alpha but aren't glass. See Direct3DDevice.cpp, search for SPECIAL_CONTROL_BLAST_MARK */
	{
		// Change the material and do max glossiness and spec_intensity
		output.ssaoMask.r = GLASS_MAT;
		output.ssaoMask.gba = 1.0;
		// Also write the normals of this surface over the current background
		output.normal.a = 1.0;
		output.ssMask.r = 0.0; // No normal mapping
		output.ssMask.g = 1.0; // White specular value
		output.ssMask.a = 1.0; // Make glass "solid" in the mask texture
	}

	return output;
}
#endif