/*
 * Laser Pointer for VR
 * (c) Leo Reyes, 2020.
 */
#include "ShaderToyDefs.h"

// LaserPointerCBuffer. This CB shares the same slot as the ShadertoyCBuffer, but
// it's using different fields.
cbuffer ConstantBuffer : register(b8)
{
	int TriggerState; // 1 = Pressed, 0 = Released
	float FOVScale;
	float2 iResolution;
	// 16 bytes
	float2 p0, p1; // Limits in uv-coords of the viewport
	// 32 bytes
	float2 contOrigin, intersection; // 2D coords of the controller's origin and the intersection
	// 48 bytes
	bool bContOrigin;		    // True if contOrigin is valid (can be displayed)
	bool bIntersection;		    // True if there is an intersection to display
	bool bHoveringOnActiveElem; // True if the cursor is hovering over an action element
	int DirectSBSEye; // if -1, then we're rendering without VR, 1 = Left Eye, 2 = Right Eye in DirectSBS mode
	// 64 bytes
	float2 v0, v1; // DEBUG (v0, v1, v2) are the vertices of the triangle where the intersection was found
	// 80 bytes
	float2 v2, uv; // DEBUG
	// 96 bytes
	bool bDebugMode;
	float cursor_radius;
	float2 lp_aspect_ratio;
	// 112 bytes
};

// Color buffer: The fully-rendered image should go in this slot. This laser pointer 
// will be an overlay on top of everything else.
Texture2D colorTex : register(t0);
SamplerState colorSampler : register(s0);

/*
float sdCircle(in vec2 p, in vec2 center, float radius)
{
	return length(p - center) - radius;
}
*/

float sdCircle(in vec2 p, float radius)
{
	return length(p) - radius;
}

/*
float sdLine(in vec2 p, in vec2 a, in vec2 b)
{
	vec2 pa = p - a, ba = b - a;
	float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
	return length(pa - ba * h);
}
*/

#undef LASER_VR_DEBUG
#ifdef LASER_VR_DEBUG
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

float debug_map(in vec2 p)
{
	return sdTriangle(p, v0, v1, v2);
}
#endif

//=====================================================

/*
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
	//d = min(d, sdCircle(p, debugPoint, 0.005));
	return d;
}
*/

struct PixelShaderInput
{
	float4 pos : SV_POSITION;
	float2 uv  : TEXCOORD;
};

struct PixelShaderOutput
{
	float4 color    : SV_TARGET0;
};

PixelShaderOutput main(PixelShaderInput input) {
	PixelShaderOutput output;
	float2 input_uv_orig = input.uv;
	vec3 color = 0.0;
	output.color = 0.0;
	
	if (DirectSBSEye == 1) // Left eye, modify the input uv
		input.uv.x *= 0.5;
	else if (DirectSBSEye == 2) // Right eye, modify input uv
		input.uv.x = input.uv.x * 0.5 + 0.5;

	// Early exit: avoid rendering outside the original viewport edges
	if (any(input.uv < p0) || any(input.uv > p1))
	{
		output.color = 0.0;
		return output;
	}

	/* if (DirectSBSEye != -1) {
		input.uv.xy *= float2(aspect_ratio, 1);
	} */
	//output.color = float4(input.uv, 0.5, 1);
	//return output;

	float3 bgColor = colorTex.Sample(colorSampler, input.uv).xyz;

	vec2 p = input_uv_orig;
	/*
	if (DirectSBSEye == -1)
		p = input.uv;
	else if (DirectSBSEye == 1)
		p = input.uv * float2(2.0, 1.0);
	else if (DirectSBSEye == 2)
		p = (input.uv - 0.5) * float2(2.0, 1.0);
	*/

	vec3 diff_col = vec3(0.9, 0.6, 0.3);
	vec3 col = 0.0;

	// DEBUG
	//output.color = float4(p, 0, 1);
	//return output;
	// DEBUG

	col = bgColor;
	float3 dotcol = bHoveringOnActiveElem ? float3(0.0, 1.0, 0.0) : float3(0.7, 0.7, 0.7);
	if (TriggerState)
		dotcol = float3(0.0, 0.0, 1.0);

	float v = 0.0, d = 10000.0;
	if (bContOrigin)
	{
		d = sdCircle(lp_aspect_ratio * (p - contOrigin), cursor_radius);
		d += 0.005;
		v += exp(-(d * d) * 5000.0);
	}

	if (bIntersection) 
	{
		d = sdCircle(lp_aspect_ratio * (p - intersection), cursor_radius);
		v += smoothstep(0.0015, 0.0, abs(d));
		// Add a second ring if we're hovering on an active element
		if (bHoveringOnActiveElem) {
			//d = sdCircle(p - intersection, cursor_radius + 0.005);
			v += smoothstep(0.0015, 0.0, abs(d - 0.005));
		}
	}

	//if (bContOrigin) 
	//{
	//	d = sdLine(p, contOrigin, intersection);
	//	d += 0.01;
	//	v += exp(-(d * d) * 7500.0);
	//}

	//v = clamp(1.2 * v, 0.0, 1.0);
	float3 pointer_col = bIntersection ? dotcol : 0.7;
	col = lerp(bgColor, pointer_col, v);

#ifdef DEBUG
	// Draw the triangle uv-color-coded
	if (bDebugMode && bIntersection && debug_map(p) < 0.001)
		col = lerp(col, float3(uv, 0.0), 0.5);
#endif

	output.color = vec4(col, 1.0);
	return output;
}