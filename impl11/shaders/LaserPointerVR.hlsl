/*
 * Laser Pointer for VR
 * (c) Leo Reyes, 2020.
 */
#include "ShaderToyDefs.h"

 // LaserPointerCBuffer
cbuffer ConstantBuffer : register(b7)
{
	float iTime, FOVScale;
	float2 iResolution;
	// 16 bytes
	float x0, y0, x1, y1; // Limits in uv-coords of the viewport
	// 32 bytes
	matrix viewMat;
	// 96 bytes
	float4 contOrigin;
	// 112 bytes
	float3 intersection;
	bool bIntersection; // True if there is an intersection to display
	// 128 bytes
};

// Color buffer: The fully-rendered image should go in this slot. This laser pointer 
// will be an overlay on top of everything else.
Texture2D colorTex : register(t0);
SamplerState colorSampler : register(s0);

static const vec3 lig = normalize(vec3(0.5, 0.0, -0.7));

float sdSphere(in vec3 p, in vec3 center, float radius)
{
	return length(p - center) - radius;
}

//=====================================================

float map(in vec3 p)
{
	//float d = sdSphere(p, contOrigin.xyz, 0.05);
	float d = 10000.0;
	if (bIntersection)
		d = min(d, sdSphere(p, intersection, 0.03));
	return d;
}

float intersect(in vec3 ro, in vec3 rd)
{
	const float maxd = 10.0;
	float h = 1.0;
	float t = 0.0;
	for (int i = 0; i < 50; i++)
	{
		if (h<0.001 || t>maxd) break;
		h = map(ro + rd * t);
		t += h;
	}

	if (t > maxd) t = -1.0;

	return t;
}

vec3 calcNormal(in vec3 pos)
{
	vec3 eps = vec3(0.002, 0.0, 0.0);

	return normalize(vec3(
		map(pos + eps.xyy) - map(pos - eps.xyy),
		map(pos + eps.yxy) - map(pos - eps.yxy),
		map(pos + eps.yyx) - map(pos - eps.yyx)));
}

struct PixelShaderInput
{
	float4 pos    : SV_POSITION;
	float4 color  : COLOR0;
	float2 uv     : TEXCOORD0;
	float4 pos3D  : COLOR1;
};

struct PixelShaderOutput
{
	float4 color    : SV_TARGET0;
};

PixelShaderOutput main(PixelShaderInput input) {
	PixelShaderOutput output;
	//vec4 fragColor = vec4(0.0, 0.0, 0.0, 1);
	vec2 fragCoord = input.uv * iResolution.xy;
	vec3 color = 0.0;
	output.color = 0.0;

	// Early exit: avoid rendering outside the original viewport edges
	if (input.uv.x < x0 || input.uv.x > x1 ||
		input.uv.y < y0 || input.uv.y > y1)
	{
		output.color = 0.0;
		return output;
	}

	float3 bgColor = colorTex.Sample(colorSampler, input.uv).xyz;

	vec2 p = (-iResolution.xy + 2.0 * fragCoord) / iResolution.y;

	vec3 ro = vec3(0.0, 0.0, -1.0);
	vec3 rd = normalize(vec3(p, 1.0));

	vec3 diff_col = vec3(0.9, 0.6, 0.3);
	vec3 spec_col = vec3(1.0, 1.0, 1.0);
	float ambient = 0.03;
	vec3 col = 0.0;
	vec3 eye = ro;

	float t = intersect(ro, rd);
	if (t > 0.0)
	{
		vec3 pos = ro + t * rd;
		vec3 nor = calcNormal(pos);
		vec3 eye_vec = normalize(eye - pos);
		vec3 refl_vec = normalize(reflect(-lig, nor));
		// Diffuse component
		col = diff_col * clamp(dot(nor, lig), 0.0, 1.0);
		// Specular component
		float spec = clamp(dot(eye_vec, refl_vec), 0.0, 1.0);
		col += spec_col * pow(spec, 16.0);
		// Ambient component
		col += ambient;
		// Gamma correction
		col = pow(clamp(col, 0.0, 1.0), 0.45);
	}
	else
		col = bgColor;

	//fragColor = vec4(col, 1.0);
	output.color = vec4(col, 1.0);
	return output;
}