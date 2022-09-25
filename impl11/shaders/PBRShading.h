#ifndef PBR_SHADING_H
#define PBR_SHADING_H

/*
 * PBR shaders based on https://www.shadertoy.com/view/4l3SWf
 */
#include "shader_common.h"
#include "RT/RTCommon.h"

#define PI 3.1415926535
#define INV_PI 0.31830988618

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

float G1V(float dotNV, float k) {
	return 1.0 / (dotNV * (1.0 - k) + k);
}

// https://twitter.com/jimhejl/status/633777619998130176
float3 ToneMapFilmic_Hejl2015(float3 hdr, float whitePt) {
	float4 vh = float4(hdr, whitePt);
	float4 va = 1.425 * vh + 0.05;
	float4 vf = (vh * va + 0.004) / (vh * (va + 0.55) + 0.0491) - 0.0821;
	return vf.rgb / vf.www;
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
	float denom = dotNH * dotNH * (rough_sqrsqr - 1.0) + 1.0;
	D = rough_sqrsqr / (PI * denom * denom);

	// Fresnel (Schlick)
	float dotLH5 = pow(1.0 - dotLH, 5.0);
	F = F0 + (1.0 - F0) * dotLH5;

	// Visibility term (G) : Smith with Schlick's approximation
	float k = rough_sqr / 2.0;
	vis = G1V(dotNL, k) * G1V(dotNV, k);

	float3 specular = /*dotNL **/ D * F * vis;

	//const float3 ambient = 0.01;
	const float3 diffuse = (albedo * INV_PI);

	//return ambient + (diffuse + specular) * light_color.xyz * dotNL;
	return (diffuse + specular) * light_color.xyz * dotNL;
}

// Main entry point for PBR shading with Ray-tracing. This is used in
// the Tech Room.
float3 addPBR_RT(in float3 position, in float3 N, in float3 FlatN, in float3 V, in float3 baseColor,
	in float metalMask, in float glossiness, in float reflectance)
{
	float3 color = 0.0;
	float roughness = 1.0 - glossiness * glossiness;
	float3 F0 = 0.16 * reflectance * reflectance * (1.0 - metalMask) + baseColor * metalMask;
	float3 albedo;
	float shadow = 1.0;
	//const float ambient = 0.05;
	const float ambient = 0.15;
	// albedo = linear_to_srgb(baseColor);
	albedo = baseColor;

	//for (int i = 0; i < globalLightsCount; i++)
	int i = 0;
	{
		//float3 L = float3(0.7, -0.7, 1);
		//float3 L = float3(0.5, 1.0, 0.7);
		//float3 light_color = 1.0;
		float3 L = globalLights[i].direction.xyz;
		float3 light_color = globalLights[i].color.w * globalLights[i].color.xyz;

		// Vector from the current point to the light source. Lights are at infinity,
		// so the current point is irrelevant.
		L = normalize(L);

		// Only do raytraced shadows for surfaces that face towards the light source.
		// If the current surface faces away, we already know it's shadowed
		const float dotLFlatN = dot(L, FlatN);
		if (bDoRaytracing && dotLFlatN > 0) {
			Ray ray;
			ray.origin = position; // position comes from pos3D. Metric, Y+ is up, Z+ is forward.
			//ray.origin = position + 0.01f * FlatN; // position comes from pos3D. Metric, Y+ is up, Z+ is forward.
			ray.dir = L;
			ray.max_dist = 1000.0f;

			Intersection inters = TraceRaySimpleHit(ray);
			if (inters.TriID > -1)
				shadow = 0.0;
		}
		if (dotLFlatN <= 0) shadow = 0.0;

		float3 col = computePBRLighting(L, light_color, position,
			N, V, albedo, roughness, F0);
		color += col;

		// shadow += softshadow(position, normalize(lights[i].pos.xyz - position), 0.02, 2.5);
	}

	//return color * shadow; // Original version
	return ambient * albedo + color * shadow; // This prevents areas in shadow from becoming full black.
}

// Main entry point for PBR shading, no Ray-tracing.
float3 addPBR(in float3 position, in float3 N, in float3 FlatN, in float3 V, in float3 baseColor,
	in float metalMask, in float glossiness, in float reflectance)
{
	float3 color = 0.0;
	float roughness = 1.0 - glossiness * glossiness;
	float3 F0 = 0.16 * reflectance * reflectance * (1.0 - metalMask) + baseColor * metalMask;
	float3 albedo;
	float shadow = 1.0;
	//const float ambient = 0.05;
	const float ambient = 0.15;
	// albedo = linear_to_srgb(baseColor);
	albedo = baseColor;

	//for (int i = 0; i < globalLightsCount; i++)
	int i = 0;
	{
		//float3 L = float3(0.7, -0.7, 1);
		//float3 L = float3(0.5, 1.0, 0.7);
		//float3 light_color = 1.0;
		float3 L = globalLights[i].direction.xyz;
		float3 light_color = globalLights[i].color.w * globalLights[i].color.xyz;

		// Vector from the current point to the light source. Lights are at infinity,
		// so the current point is irrelevant.
		L = normalize(L);

		// Only do raytraced shadows for surfaces that face towards the light source.
		// If the current surface faces away, we already know it's shadowed
		const float dotLFlatN = dot(L, FlatN);
		if (dotLFlatN <= 0) shadow = 0.0;

		float3 col = computePBRLighting(L, light_color, position,
			N, V, albedo, roughness, F0);
		color += col;

		// shadow += softshadow(position, normalize(lights[i].pos.xyz - position), 0.02, 2.5);
	}

	//return color * shadow; // Original version
	return ambient * albedo + color * shadow; // This prevents areas in shadow from becoming full black.
}
#endif