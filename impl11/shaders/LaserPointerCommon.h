/*
 * Laser Pointer for VR
 * (c) Leo Reyes, 2020.
 */
#include "ShaderToyDefs.h"

 // LaserPointerCBuffer. This CB shares the same slot as the ShadertoyCBuffer, but
 // it's using different fields.
cbuffer ConstantBuffer : register(b8)
{
	int TriggerState0;           // 1 = Pressed, 0 = Released
	int TriggerState1;
	float2 iResolution;
	// 16 bytes
	float2 p0, p1;               // Limits in uv-coords of the viewport
	// 32 bytes
	bool bContOrigin0;           // True if contOrigin is valid (can be displayed)
	bool bContOrigin1;
	bool bHoveringOnActiveElem0; // True if cursor0 is hovering over an action element
	bool bHoveringOnActiveElem1; // True if cursor1 is hovering over an action element
	// 48 bytes
	float4 contOriginL0;         // contIdx:0, contOrigin.xy --> uv-coords, contOrigin.z --> metric depth
	float4 contOriginR0;
	// 80 bytes
	float4 contOriginL1;         // contIdx:1,  contOrigin.xy --> uv-coords, contOrigin.z --> metric depth
	float4 contOriginR1;
	// 112 bytes
	float4 intersectionL0;       // contIdx:0, intersection.xy --> uv-coords, intersection.z --> metric depth
	float4 intersectionR0;       // contIdx:0
	// 144 bytes
	float4 intersectionL1;       // contIdx:1, intersection.xy --> uv-coords, intersection.z --> metric depth
	float4 intersectionR1;       // contIdx:1
	// 176 bytes
	float2 uv0;
	float2 uv1;
	// 192 bytes
	bool   bDebugMode;
	int    ac_unused0;
	float  cursor_radius0;
	float  cursor_radius1;
	// 208 bytes
	float inters_radius0;
	float inters_radius1;
	bool bDisplayLine0;
	bool bDisplayLine1;
	// 224 bytes
	bool bIntersection0; // True if there is an intersection to display
	bool bIntersection1;
	float2 lp_aspect_ratio;
	// 240 bytes
	float2 v0L, v1L; // DEBUG (v0, v1, v2) are the vertices of the triangle where the intersection was found
	// 256 bytes
	float2 v2L, v0R; // DEBUG
	float2 v1R, v2R;
	// 288 bytes
};

#ifdef INSTANCED_RENDERING
// Color buffer: The fully-rendered image should go in this slot. This laser pointer
// will be an overlay on top of everything else.
Texture2DArray colorTex     : register(t0);
SamplerState   colorSampler : register(s0);
// The position/depth buffer
Texture2DArray texPos       : register(t1);
#else
// Color buffer: The fully-rendered image should go in this slot. This laser pointer
// will be an overlay on top of everything else.
Texture2D    colorTex     : register(t0);
SamplerState colorSampler : register(s0);
// The position/depth buffer
Texture2D    texPos       : register(t1);
#endif

struct PixelShaderInput
{
	float4 pos  : SV_POSITION;
	float2 uv   : TEXCOORD;
#ifdef INSTANCED_RENDERING
	uint viewId : SV_RenderTargetArrayIndex;
#endif
};

struct PixelShaderOutput
{
	float4 color : SV_TARGET0;
};

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

float sdLine(in vec2 p, in vec2 a, in vec2 b)
{
	vec2 pa = lp_aspect_ratio * (p - a);
	vec2 ba = lp_aspect_ratio * (b - a);
	float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
	return length(pa - ba * h);
}

#undef LASER_VR_DEBUG
#ifdef LASER_VR_DEBUG
float sdTriangle(in vec2 p, in vec2 p0, in vec2 p1, in vec2 p2)
{
	vec2 e0 = p1 - p0, e1 = p2 - p1, e2 = p0 - p2;
	vec2 v0 = p - p0, v1 = p - p1, v2 = p - p2;
	vec2 pq0 = v0 - e0 * clamp(dot(v0, e0) / dot(e0, e0), 0.0, 1.0);
	vec2 pq1 = v1 - e1 * clamp(dot(v1, e1) / dot(e1, e1), 0.0, 1.0);
	vec2 pq2 = v2 - e2 * clamp(dot(v2, e2) / dot(e2, e2), 0.0, 1.0);
	float s = sign(e0.x * e2.y - e0.y * e2.x);
	vec2 d = min(min(vec2(dot(pq0, pq0), s * (v0.x * e0.y - v0.y * e0.x)),
		vec2(dot(pq1, pq1), s * (v1.x * e1.y - v1.y * e1.x))),
		vec2(dot(pq2, pq2), s * (v2.x * e2.y - v2.y * e2.x)));
	return -sqrt(d.x) * sign(d.y);
}

float debug_map(in vec2 p, in float2 v0, in float2 v1, in float2 v2)
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

void RenderCursor(
	const vec3   bgColor,
	const vec3   pos3D,
	const vec2   p,
	const int    TriggerState,
	const bool   bHoveringOnActiveElem,
	const bool   bContOrigin,
	const bool   bDisplayLine,
	const bool   bIntersection,
	const float4 contOrigin,
	const float4 intersection,
	const float  cursor_radius,
	const float  inters_radius,
	const uint   viewId,
	out   float3 pointer_col,
	out   float  blend)
{
	float3 dotcol = bHoveringOnActiveElem ? float3(0.0, 1.0, 0.0) : 1.0;
	if (TriggerState)
		dotcol = float3(0.1, 0.1, 1.0);

	float v = 0.0, d = 10000.0;

	// Display the cursor origin
	if (bContOrigin && bDisplayLine)
	{
		d = sdCircle(lp_aspect_ratio * (p - contOrigin.xy), cursor_radius);
		v += smoothstep(0.0015, 0.0, abs(d));
	}

	// Display the intersection
	if (bIntersection &&
		// Do not display the intersection if it's occluded
		intersection.z <= pos3D.z + 0.01)
	{
		d = sdCircle(lp_aspect_ratio * (p - intersection.xy), inters_radius);
		v += smoothstep(0.0015, 0.0, abs(d));
		// Add a second ring if we're hovering on an active element
		if (bHoveringOnActiveElem) {
			//d = sdCircle(p - intersection, cursor_radius + 0.005);
			//v += smoothstep(0.0015, 0.0, abs(d - 0.005));
			v += smoothstep(0.0015, 0.0, abs(d - 0.0025));
		}
	}

	if (bContOrigin && bDisplayLine)
	{
		d = sdLine(p, contOrigin.xy, intersection.xy);

		// Compute the vector from the controller origin to the intersection
		const float2 dir0 = intersection.xy - contOrigin.xy;
		const float D = length(dir0);
		float2 dir = dir0 / D;
		// Compute the orthogonal vector
		dir = float2(-dir.y, dir.x);
		// Find the closest point on the line to the current pixel (p)
		const float2 q = p + d * dir;
		// Find the normalized distance from the closest point on the line (q) to the controller origin:
		const float dist = length(q - contOrigin.xy) / D;
		// Find the depth for the point q on the line:
		const float lineZ = lerp(contOrigin.z, intersection.z, dist);
		// Interpolate the cursor radius at point q
		const float lineRadius = lerp(cursor_radius, inters_radius, dist);
		// Display the original line, with depth occlusion:
		if (lineZ <= pos3D.z + 0.01)
		{
			// DEBUG: Draw a really small circle around q. This ends up looking like a thin line
			//d = sdCircle(lp_aspect_ratio * (p - q), 0.001f);
			//v += smoothstep(0.0045, 0.0, abs(d));

			v += smoothstep(lineRadius + 0.0015, lineRadius, abs(d));
		}
		// v controls the blending, so we can apply transparency by modulating it:
		if (!bIntersection)
			v *= (1.0 - dist);
	}

	// v controls the blending, so we can apply transparency by modulating it
	v *= 0.7;

	//v = clamp(1.2 * v, 0.0, 1.0);
	pointer_col = bIntersection ? dotcol : 1.0;
	blend = v;

#ifdef LASER_VR_DEBUG
	// Draw the triangle, uv-color-coded
	//if (bDebugMode && bIntersection && debug_map(p) < 0.001)
	if (bDebugMode)
	{
		const float2 v0 = (viewId == 0) ? v0L : v0R;
		const float2 v1 = (viewId == 0) ? v1L : v1R;
		const float2 v2 = (viewId == 0) ? v2L : v2R;

		if (debug_map(p, v0, v1, v2) <= 0.0)
		{
			pointer_col += float3(0.5, 0, 0);
			blend = 0.3f;
		}
		else if (debug_map(p, v0, v1, v2) < 0.001)
		{
			pointer_col += 1;
			blend = 0.5f;
		}
	}
#endif
}

PixelShaderOutput main(PixelShaderInput input) {
	PixelShaderOutput output;
	//float2 input_uv_orig = input.uv;
	vec3 color = 0.0;
	output.color = 0.0;

	//if (DirectSBSEye == 1) // Left eye, modify the input uv
	//	input.uv.x *= 0.5;
	//else if (DirectSBSEye == 2) // Right eye, modify input uv
	//	input.uv.x = input.uv.x * 0.5 + 0.5;

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

	// DEBUG
	//output.color = float4(p, 0, 1);
	//return output;
	// DEBUG

#ifdef INSTANCED_RENDERING
	float3 bgColor = colorTex.Sample(colorSampler, float3(input.uv, input.viewId)).xyz;
	float3 pos3D   = texPos.Sample(colorSampler,   float3(input.uv, input.viewId)).xyz;

	const float4 contOrigin0   = input.viewId == 0 ? contOriginL0   : contOriginR0;
	const float4 intersection0 = input.viewId == 0 ? intersectionL0 : intersectionR0;

	const float4 contOrigin1   = input.viewId == 0 ? contOriginL1   : contOriginR1;
	const float4 intersection1 = input.viewId == 0 ? intersectionL1 : intersectionR1;
	const uint   viewId        = input.viewId;
#else
	float3 bgColor = colorTex.Sample(colorSampler, input.uv).xyz;
	float3 pos3D   = texPos.Sample(colorSampler,   input.uv).xyz;

	const float4 contOrigin0   = contOriginL0;
	const float4 intersection0 = intersectionL0;

	const float4 contOrigin1   = contOriginL1;
	const float4 intersection1 = intersectionL1;
	const uint   viewId        = 0;
#endif

	float3 pointer_col;
	float blend;
	RenderCursor(bgColor, pos3D, input.uv,
	             TriggerState0, bHoveringOnActiveElem0,
	             bContOrigin0, bDisplayLine0, bIntersection0,
	             contOrigin0, intersection0,
	             cursor_radius0, inters_radius0, viewId,
	             pointer_col, blend);
	output.color.rgb = lerp(bgColor, pointer_col, blend);

	RenderCursor(bgColor, pos3D, input.uv,
	             TriggerState1, bHoveringOnActiveElem1,
	             bContOrigin1, bDisplayLine1, bIntersection1,
	             contOrigin1, intersection1,
	             cursor_radius1, inters_radius1, viewId,
	             pointer_col, blend);
	output.color.rgb = lerp(output.color.rgb, pointer_col, blend);
	output.color.a = 1;
	return output;
}
