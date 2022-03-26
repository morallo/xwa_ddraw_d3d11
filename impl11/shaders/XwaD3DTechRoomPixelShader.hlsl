/*
 * Original Pixel Shader: Jeremy Ansel, 2022.
 * Updated for XWA Effects by Leo Reyes 2022.
 * PBR shaders based on https://www.shadertoy.com/view/4l3SWf
 */
#include "XwaD3dCommon.hlsl"
#include "HSV.h"
#include "shader_common.h"
#include "shading_system.h"
#include "PixelShaderTextureCommon.h"

Texture2D texture0 : register(t0); // This is the regular color texture
Texture2D texture1 : register(t1); // If present, this is the light texture
SamplerState sampler0 : register(s0);
// Normal Map, slot 13
Texture2D normalMap : register(t13);

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

// https://en.wikipedia.org/wiki/SRGB
float3 srgb_to_linear(float3 srgb) {
	float a = 0.055;
	float b = 0.04045;
	float3 linear_lo = srgb / 12.92;
	float3 linear_hi = pow(abs((srgb + a) / (1.0 + a)), 2.4);
	return float3(
		srgb.r > b ? linear_hi.r : linear_lo.r,
		srgb.g > b ? linear_hi.g : linear_lo.g,
		srgb.b > b ? linear_hi.b : linear_lo.b);
}

// https://en.wikipedia.org/wiki/SRGB
float3 linear_to_srgb(float3 linear_rgb) {
	float a = 0.055;
	float b = 0.0031308;
	float3 srgb_lo = 12.92 * linear_rgb;
	float3 srgb_hi = (1.0 + a) * pow(abs(linear_rgb), 0.416667) - a; // 1/2.4 = 0.416667
	return float3(
		linear_rgb.r > b ? srgb_hi.r : srgb_lo.r,
		linear_rgb.g > b ? srgb_hi.g : srgb_lo.g,
		linear_rgb.b > b ? srgb_hi.b : srgb_lo.b);
}

// https://twitter.com/jimhejl/status/633777619998130176
float3 ToneMapFilmic_Hejl2015(float3 hdr, float whitePt) {
	float4 vh = float4(hdr, whitePt);
	float4 va = 1.425 * vh + 0.05;
	float4 vf = (vh * va + 0.004) / (vh * (va + 0.55) + 0.0491) - 0.0821;
	return vf.rgb / vf.www;
}

float G1V(float dotNV, float k) {
	return 1.0 / (dotNV*(1.0 - k) + k);
}

// L: is the light direction, from the current point (position) to the light
// position: the current 3D position to be shaded
float3 computePBRLighting(in float3 L, in float3 light_color, in float3 position, in float3 N, in float3 V, 
	in float3 albedo, in float roughness, in float3 F0)
{
	float rough_sqr = roughness * roughness;
	//float3 L = normalize(light.pos.xyz - position);
	float3 H = normalize(V + L);

	float dotNL = saturate(dot(N, L));
	float dotNV = saturate(dot(N, V));
	float dotNH = saturate(dot(N, H));
	float dotLH = saturate(dot(L, H));

	float D, vis;
	float3 F;

	// NDF : GGX
	float rough_sqrsqr = rough_sqr * rough_sqr;
	float pi = 3.1415926535;
	float denom = dotNH * dotNH * (rough_sqrsqr - 1.0) + 1.0;
	D = rough_sqrsqr / (pi * denom * denom);

	// Fresnel (Schlick)
	float dotLH5 = pow(1.0 - dotLH, 5.0);
	F = F0 + (1.0 - F0) * dotLH5;

	// Visibility term (G) : Smith with Schlick's approximation
	float k = rough_sqr / 2.0;
	vis = G1V(dotNL, k) * G1V(dotNV, k);

	float3 specular = /*dotNL **/ D * F * vis;

	//const float3 ambient = 0.01;

	const float invPi = 0.31830988618;
	const float3 diffuse = (albedo * invPi);

	//return ambient + (diffuse + specular) * light_color.xyz * dotNL;
	return (diffuse + specular) * light_color.xyz * dotNL;
}

float3 addPBR(in float3 position, in float3 N, in float3 V, in float3 baseColor,
	in float metalMask, in float glossiness, in float reflectance)
{
	float3 color = 0.0;
	float roughness = 1.0 - glossiness * glossiness;
	float3 F0 = 0.16 * reflectance * reflectance * (1.0 - metalMask) + baseColor * metalMask;
	float3 albedo;
	const float shadow = 1.0;
	const float ambient = 0.05;
	// albedo = linear_to_srgb(baseColor);
	albedo = baseColor;

	for (int i = 0; i < globalLightsCount; i++)
	{
		//float3 L = float3(0.7, -0.7, 1);
		//float3 L = float3(0.5, 1.0, 0.7);
		//float3 light_color = 1.0;
		float3 L = globalLights[i].direction.xyz;
		float3 light_color = globalLights[i].color.w * globalLights[i].color.xyz;

		L = normalize(L);
		float3 col = computePBRLighting(L, light_color, position,
			N, V, albedo, roughness, F0);
		color += col;

		// shadow += softshadow(position, normalize(lights[i].pos.xyz - position), 0.02, 2.5);
	}

	//return color * shadow; // Original version
	return ambient * albedo + color * shadow; // This prevents areas in shadow from becoming full black.
}

PixelShaderOutput main(PixelShaderInput input)
{
	PixelShaderOutput output;
	// This is the per-vertex Gouraud-shaded color coming from the VR:
	//float4 color			= float4(input.color.xyz, 1.0f);
	float4 texelColor = texture0.Sample(sampler0, input.tex);
	float3 normalMapColor = bDoNormalMapping ? normalMap.Sample(sampler0, input.tex).rgb : 0;
	uint bIsBlastMark = special_control & SPECIAL_CONTROL_BLAST_MARK;
	uint ExclusiveMask = special_control & SPECIAL_CONTROL_EXCLUSIVE_MASK;
	if (bIsBlastMark)
		texelColor = texture0.Sample(sampler0, (input.tex * 0.35) + 0.3);

	float  alpha = texelColor.w;
	float3 P = input.pos3D.xyz;
	float  SSAOAlpha = saturate(min(alpha - fSSAOAlphaOfs, fPosNormalAlpha));

	// Zero-out the bloom mask and provide default output values
	output.bloom = 0;
	output.color = output.color = float4(brightness * texelColor.xyz, texelColor.w);
	output.pos3D = float4(P, SSAOAlpha);
	output.ssMask = 0;

	float3 N = normalize(input.normal.xyz);
	N.y = -N.y; // Invert the Y axis, originally Y+ is down
	N.z = -N.z;
	output.normal = float4(N, SSAOAlpha);

	float3 T = normalize(input.tangent.xyz);
	T.y = -T.y; // Invert the Y axis, originally Y+ is down
	T.z = -T.z;
	float3 B = cross(T, N);
	float3x3 TBN = float3x3(T, B, N);

	if (bDoNormalMapping) {
		const float3 NM = normalize(mul(0.5 * (normalMapColor - 0.5), TBN));
		N = lerp(N, NM, fNMIntensity);
	}

	if (ExclusiveMask == SPECIAL_CONTROL_GRAYSCALE)
		texelColor = float4(0.7, 0.7, 0.7, 1.0);

	// ssaoMask: Material, Glossiness, Specular Intensity
	output.ssaoMask = float4(fSSAOMaskVal, fGlossiness, fSpecInt, alpha);
	// SS Mask: Normal Mapping Intensity, Specular Value, Shadeless
	output.ssMask = float4(fNMIntensity, fSpecVal, fAmbient, alpha);

	// DEBUG: Display the normal map
	/*
	if (bDoNormalMapping)
	{
		output.color = float4(normalMapColor, 1);
		return output;
	}
	*/
	// DEBUG: Display the tangent map
	/*
	if (ExclusiveMask == SPECIAL_CONTROL_GRAYSCALE) {
		output.color = float4(T, 1.0);
		//output.color = float4(input.tex.xy, 0, 1);
		return output;
	}
	*/
	// DEBUG

	const float exposure = 1.0;
	//float glossiness = output.ssaoMask.g + 0.5;
	float metallicity = 0.25;
	float glossiness = 0.7;
	float reflectance = 0.6;
	float final_alpha = alpha;
	if (alpha < 0.98)
	{
		glossiness = 0.92;
		reflectance = 1.0;
	}
	float3 eye_vec = normalize(-output.pos3D.xyz); // normalize(eye - pos3D); // Vector from pos3D to the eye (0,0,0)
	float3 col = addPBR(P, N, -eye_vec, srgb_to_linear(texelColor.rgb),
		metallicity,
		glossiness, // Glossiness: 0 matte, 1 glossy/glass
		reflectance
	);
	output.color.rgb = linear_to_srgb(ToneMapFilmic_Hejl2015(col * exposure, 1.0));
	if (alpha < 0.95)
		final_alpha = max(alpha, dot(output.color.rgb, 0.333));
	output.color.a = final_alpha;

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
