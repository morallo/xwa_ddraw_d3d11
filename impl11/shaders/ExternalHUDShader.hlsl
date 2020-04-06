/*
 * Hyperspace jump trails.
 *
 * You can use it under the terms of the MIT license, see LICENSE.TXT
 * (free to use even in commercial projects, attribution required)
 *
 * Adapted from 	https://www.shadertoy.com/view/MlKBWw by Leo Reyes
 * Shadertoy implementation: https://www.shadertoy.com/view/3l3Gz8
 *
 * Lens flare from: https://www.shadertoy.com/view/XdfXRX
 * and: https://www.shadertoy.com/view/4sX3Rs
 * (c) Leo Reyes, 2019.
 */

#include "ShaderToyDefs.h"

 // The background texture
Texture2D    bgTex     : register(t0);
SamplerState bgSampler : register(s0);

#define cursor_radius 0.04

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
};

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

float sdLine(in vec2 p, in vec2 a, in vec2 b, in float ring)
{
	vec2 pa = p - a, ba = b - a;
	float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
	return length(pa - ba * h) - ring;
}

vec3 lensflare(vec3 uv, vec3 pos, float flare_size, float ang_offset)
{
	float z = uv.z / length(uv.xy);
	vec2 main = uv.xy - pos.xy;
	float dist = length(main);
	float num_points = 2.71;
	float disk_size = 0.2;
	float inv_size = 1.0 / flare_size;
	float ang = atan2(main.y, main.x) + ang_offset;
	float fade = (z < 0.0) ? -z : 1.0;

	float f0 = 1.0 / (dist * inv_size + 1.0);
	f0 = f0 + f0 * (0.1 * sin((sin(ang*2.0 + pos.x)*4.0 - cos(ang*3.0 + pos.y)) * num_points) + disk_size);
	if (z < 0.0) // Remove the flare on the back
		return clamp(mix(f0, 0.0, 0.75 * fade), 0.0, 1.0);
	else
		return f0;
}

vec3 cc(vec3 color, float factor, float factor2) // color modifier
{
	float w = color.x + color.y + color.z;
	return mix(color, w * factor, w * factor2);
}

float sdCircle(in vec2 p, in vec2 center, float radius)
{
	return length(p - center) - radius;
}

PixelShaderOutput main(PixelShaderInput input) {
	PixelShaderOutput output;
	vec4 fragColor = vec4(0.0, 0.0, 0.0, 1);
	vec2 fragCoord = input.uv * iResolution.xy;
	vec3 color = 0.0;
	output.color = bgTex.Sample(bgSampler, input.uv);

	// DEBUG
	//output.color = float4(input.uv, 0.0, 1.0);
	//output.color.b += 0.1;
	//return output;
	// DEBUG

	// Early exit: avoid rendering outside the original viewport edges
	if (any(input.uv < p0) ||
		any(input.uv > p1))
	{
		return output;
	}

	vec2 p = (2.0 * fragCoord.xy - iResolution.xy) / min(iResolution.x, iResolution.y);
	//p += vec2(0, y_center); // In XWA the aiming HUD is not at the screen's center

	float p_len = length(p);
	vec3 v = vec3(p, -FOVscale);
	v = mul(viewMat, vec4(v, 0.0)).xyz;

	float col = 0.0;
	float d = sdCircle(v.xy, vec2(0.0, 0.0), cursor_radius);
	col += smoothstep(0.0015, 0.0, abs(d));

	output.color.rgb += col;
	return output;
}