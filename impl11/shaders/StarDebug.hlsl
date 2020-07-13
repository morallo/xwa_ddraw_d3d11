/*
 * Star Debug shader.
 *
 * You can use it under the terms of the MIT license, see LICENSE.TXT
 * (free to use even in commercial projects, attribution required)
 *
 * (c) Leo Reyes, 2020.
 */

#include "ShaderToyDefs.h"
#include "shading_system.h"
#include "ShadertoyCBuffer.h"
#include "metric_common.h"

 // The background texture
Texture2D    bgTex     : register(t0);
SamplerState bgSampler : register(s0);

#define cursor_radius 0.04
//#define thickness 0.02 
//#define scale 2.0
#define thickness 0.007
#define scale 1.25

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

float sdLine(in vec2 p, in vec2 a, in vec2 b)
{
	vec2 pa = p - a, ba = b - a;
	float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
	return length(pa - ba * h);
}

float sdCircle(in vec2 p, in vec2 center, float radius)
{
	return length(p - center) - radius;
}

// Display the current MainLight, using regular UV post proc coords
PixelShaderOutput main(PixelShaderInput input) {
	/* This shader doesn't work well in VR mode. */
	PixelShaderOutput output;
	vec3 color = 0.0;
	output.color = 0;

	if (VRmode == 0) 
		output.color = bgTex.Sample(bgSampler, input.uv);
	else if (VRmode == 1) {
		if (input.uv.x <= 0.5)
			output.color = bgTex.Sample(bgSampler, input.uv * float2(0.5, 1.0));
		else
			output.color = bgTex.Sample(bgSampler, input.uv * float2(0.5, 1.0) + float2(0.5, 0.0));
	}

	// Early exit: avoid rendering outside the original viewport edges
	if (any(input.uv < p0) || any(input.uv > p1))
		return output;

	if (MainLight.z < 0.0) // Skip lights behind the camera
		return output;

	float3 col = float3(1.0, 0.1, 0.1); // Marker color
	float d, dm;
	d = sdCircle(input.uv, MainLight.xy, scale * cursor_radius);
	dm = smoothstep(thickness * 0.5, 0.0, abs(d)); // Outer ring
	dm += smoothstep(thickness * 0.5, 0.0, abs(d + 0.5 * scale * (cursor_radius - 0.001))); // Center dot

	dm = clamp(dm, 0.0, 1.0);
	col *= dm;
	output.color.rgb = lerp(output.color.rgb, col, 0.8 * dm);
	return output;
}
