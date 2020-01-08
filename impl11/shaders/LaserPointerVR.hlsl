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
	float2 contOrigin, intersection;
	// 112 bytes
	bool bContOrigin; // True if contOrigin is valid
	bool bIntersection; // True if there is an intersection to display
	float2 debugPoint;
	// 128 bytes
	float2 v0, v1; // DEBUG
	// 144 bytes
	float2 v2, uv; // DEBUG
};

// Color buffer: The fully-rendered image should go in this slot. This laser pointer 
// will be an overlay on top of everything else.
Texture2D colorTex : register(t0);
SamplerState colorSampler : register(s0);

/*
static const vec3 lig = normalize(vec3(0.5, 0.0, -0.7));

float sdSphere(in vec3 p, in vec3 center, float radius)
{
	return length(p - center) - radius;
}
*/

float sdCircle(in vec2 p, in vec2 center, float radius)
{
	return length(p - center) - radius;
}

float sdLine(in vec2 p, in vec2 a, in vec2 b)
{
	vec2 pa = p - a, ba = b - a;
	float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
	return length(pa - ba * h);
}

float sdTriangle(in vec2 p, in vec2 p0, in vec2 p1, in vec2 p2)
{
	vec2 e0 = p1 - p0, e1 = p2 - p1, e2 = p0 - p2;
	vec2 v0 = p - p0, v1 = p - p1, v2 = p - p2;
	vec2 pq0 = v0 - e0 * clamp(dot(v0, e0) / dot(e0, e0), 0.0, 1.0);
	vec2 pq1 = v1 - e1 * clamp(dot(v1, e1) / dot(e1, e1), 0.0, 1.0);
	vec2 pq2 = v2 - e2 * clamp(dot(v2, e2) / dot(e2, e2), 0.0, 1.0);
	float s = sign(e0.x*e2.y - e0.y*e2.x);
	vec2 d = min(min(vec2(dot(pq0, pq0), s*(v0.x*e0.y - v0.y*e0.x)),
		vec2(dot(pq1, pq1), s*(v1.x*e1.y - v1.y*e1.x))),
		vec2(dot(pq2, pq2), s*(v2.x*e2.y - v2.y*e2.x)));
	return -sqrt(d.x)*sign(d.y);
}

//=====================================================

/*
float map3D(in vec3 p)
{
	//float d = sdSphere(p, contOrigin.xyz, 0.05);
	float d = 10000.0;
	if (bIntersection)
		d = min(d, sdSphere(p, intersection, 0.03));
	return d;
}
*/

float map(in vec2 p)
{
	float d = 10000.0;
	if (bIntersection)
		d = sdCircle(p, intersection, 0.0025);
	if (bContOrigin)
		d = min(d, sdCircle(p, contOrigin, 0.0075));
	if (bContOrigin && bIntersection)
		d = min(d, sdLine(p, contOrigin, intersection) - 0.001);
	// Display the debug point too
	d = min(d, sdCircle(p, debugPoint, 0.005));
	return d;
}

float debug_map(in vec2 p) 
{
	return sdTriangle(p, v0, v1, v2);
}

/*
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
*/

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

	//vec2 p = fragCoord / iResolution.xy;
	vec2 p = input.uv;

	vec3 diff_col = vec3(0.9, 0.6, 0.3);
	vec3 col = 0.0;

	// DEBUG
	//output.color = float4(p, 0, 1);
	//return output;
	// DEBUG

	//float t = intersect(ro, rd);
	col = bgColor;

	// Draw the ray and dot:
	float t = map(p);
	if (t < 0.001)
	{
		float3 pointer_col = bIntersection ? float3(1.0, 0.0, 0.0) : float3(0.7, 0.7, 0.7);
		col = lerp(bgColor, pointer_col, 0.75);
	}
	
	// Draw the triangle uv-color-coded
	if (bIntersection && debug_map(p) < 0.001)
		col = lerp(col, float3(uv, 0.0), 0.5);

	//col.b += 0.1;

	// Gamma correction
	//col = pow(clamp(col, 0.0, 1.0), 0.45);

	//fragColor = vec4(col, 1.0);
	output.color = vec4(col, 1.0);
	return output;
}