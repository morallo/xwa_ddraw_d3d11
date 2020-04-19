// Copyright (c) 2020 Leo Reyes
// Licensed under the MIT license. See LICENSE.txt
// With contributions from the collective minds at Shadertoy

#include "shader_common.h"
#include "HSV.h"
#include "shading_system.h"
#include "PixelShaderTextureCommon.h"
#include "ShaderToyDefs.h"

#define SPEED 0.25
#define PERIOD 4.0 // Increase this to make more/thinner rays

Texture2D    texture0 : register(t0);
SamplerState sampler0 : register(s0);

struct PixelShaderInput
{
	float4 pos    : SV_POSITION;
	float4 color  : COLOR0;
	float2 tex    : TEXCOORD0;
	float4 pos3D  : COLOR1;
	float4 normal : NORMAL;
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

// ShadertoyCBuffer
cbuffer ConstantBuffer : register(b7)
{
	float iTime, twirl, bloom_strength, srand;
	// 16 bytes
	float2 iResolution;
	uint bDirectSBS;
	float y_center;
	// 32 bytes
	float2 p0, p1; // Limits in uv-coords of the viewport
	// 48 bytes
	matrix viewMat;
	// 112 bytes
	uint bDisneyStyle, hyperspace_phase;
	float tunnel_speed, FOVscale;
	// 128 bytes
	float2 SunCoords; // Coordinates in desktop resolution
	float2 LightPos; // Coordinates of the associated light
	// 144 bytes
	float4 SunColor;
	// 160 bytes
};

// 3D noise from: https://www.shadertoy.com/view/4sfGzS
float hash(vec3 p)
{
	p = fract(p*0.3183099 + 0.1);
	p *= 17.0;
	return fract(p.x*p.y*p.z*(p.x + p.y + p.z));
}

float noise(in vec3 x)
{
	vec3 i = floor(x);
	vec3 f = fract(x);
	f = f * f*(3.0 - 2.0*f);

	return mix(mix(mix(hash(i + vec3(0, 0, 0)),
		hash(i + vec3(1, 0, 0)), f.x),
		mix(hash(i + vec3(0, 1, 0)),
			hash(i + vec3(1, 1, 0)), f.x), f.y),
		mix(mix(hash(i + vec3(0, 0, 1)),
			hash(i + vec3(1, 0, 1)), f.x),
			mix(hash(i + vec3(0, 1, 1)),
				hash(i + vec3(1, 1, 1)), f.x), f.y), f.z);
}

static float3x3 m = float3x3(
	 0.00,  0.80,  0.60,
	-0.80,  0.36, -0.48,
	-0.60, -0.48,  0.64
);

float fbm(vec3 q) {
	float f;
	f  = 0.5000*noise(q); q = mul(m, q*2.01);
	f += 0.2500*noise(q); q = mul(m, q*2.02);
	f += 0.1250*noise(q); q = mul(m, q*2.03);
	f += 0.0625*noise(q); q = mul(m, q*2.01);
	return f;
}

PixelShaderOutput main(PixelShaderInput input)
{
	PixelShaderOutput output;
	float4 texelColor = texture0.Sample(sampler0, input.tex);
	float  alpha = texelColor.w;
	float3 diffuse = lerp(input.color.xyz, 1.0, fDisableDiffuse);
	float3 P = input.pos3D.xyz;
	float  SSAOAlpha = saturate(min(alpha - fSSAOAlphaOfs, fPosNormalAlpha));
	// Zero-out the bloom mask.
	output.bloom = 0;
	output.color = texelColor;
	output.pos3D = float4(P, SSAOAlpha);
	output.ssMask = 0;

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
	// SS Mask: Normal Mapping Intensity, Specular Value, unused
	output.ssMask = float4(fNMIntensity, fSpecVal, 0.0, alpha);

	// Sun Shader
	// Disable depth-buffer write, etc. for sun textures
	output.pos3D = 0;
	output.normal = 0;
	output.ssaoMask = 0;
	output.ssMask = 0;

	// SunColor.a selects either white (0) or a light color (1) specified by SunColor.rgb:
	float3 corona_color = lerp(1.0, SunColor.rgb, SunColor.a);
	float2 v = float2(input.tex.xy - 0.5);
	const float V_2 = dot(v, v);
	float intensity = saturate(pow(0.01 / V_2, 1.8));
	//const float3 light_color = float3(0.3, 0.3, 1.0);
	//const float3 light_color = 1.0;
	// Center disk:
	output.color = intensity;
	output.bloom = float4(fBloomStrength * output.color.xyz * corona_color, intensity);

	// Add the corona
	float2 pos = 8.0 * v;
	float r = length(pos);
	v = pos / r;
	r -= 1.0;

	//float f = fbm(vec3(PERIOD * p, SPEED * t - 0.5 * r));
	// This version shows straight lines:
	//float f = fbm(vec3(PERIOD * p, SPEED * t));
	float f = fbm(vec3(PERIOD * v, SPEED * iTime - 0.25 * r));
	// Fade out the noise:
	f = clamp(f + 1.5 * exp(-0.5 * r) - 0.8, 0.0, 1.0);
	float cursor = f * 0.75;
	cursor *= cursor * cursor;
	corona_color = mix(corona_color, 1.0, cursor);
	vec3 color = 0.9 * f * corona_color;

	//r = clamp(r * 6.0, 0.0, 1.0);
	//col += r * clamp(color, 0.0, 1.0);
	intensity = exp(-r * 0.001) * 0.9 * f;
	output.color.rgb += exp(-r * 0.001) * clamp(color, 0.0, 1.0);
	output.color.a += intensity;

	//output.color.xyz *= output.color.xyz; // Gamma compensation?
	return output;
}