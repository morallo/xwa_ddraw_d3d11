/*
 * Sun/Star Shader.
 *
 * You can use this shader under the terms of the MIT license, see LICENSE.TXT
 * (free to use even in commercial projects, attribution required)
 *
 * (c) Leo Reyes, 2020.
 */

#include "ShaderToyDefs.h"
#include "shading_system.h"

 // The background texture
Texture2D    bgTex     : register(t0);
SamplerState bgSampler : register(s0);

#define cursor_radius 0.04
#define thickness 0.02 //0.007
#define scale 2.0

/*
	// saturation test:

	vec3 col = vec3(0.0, 0.0, 1.0);
	float g = dot(vec3(0.333), col);
	vec3 gray = vec3(g);
	// saturation test
	float sat = 2.0;
	vec3 final_col = gray + sat * (col - gray);

 */

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
	float SunX, SunY, sun_intensity, st_unused1;
	// 144 bytes
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

float sdLine(in vec2 p, in vec2 a, in vec2 b)
{
	vec2 pa = p - a, ba = b - a;
	float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
	return length(pa - ba * h);
}

float lensflare(vec2 uv, vec2 pos, float flare_size, float ang_offset)
{
	vec2 main = uv - pos;
	float dist = length(main);
	float num_points = 2.71;
	float disk_size = 0.2;
	float inv_size = 1.0 / flare_size;
	float ang = atan2(main.y, main.x) + ang_offset;

	float f0 = 1.0 / (dist * inv_size + 1.0);
	f0 = f0 + f0 * (0.1 * sin((sin(ang*2.0 + pos.x)*4.0 - cos(ang*3.0 + pos.y)) * num_points) + disk_size);
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
	if (any(input.uv < p0) || any(input.uv > p1))
		return output;

	float d, dm;

	vec2 p = (2.0 * fragCoord.xy - iResolution.xy) / min(iResolution.x, iResolution.y);
	p += vec2(0, y_center); // In XWA the aiming HUD is not at the screen's center in cockpit view
	vec3 v = vec3(p.x, -p.y, -FOVscale);
	v = mul(viewMat, vec4(v, 0.0)).xyz;
	//vec3 v = vec3(p, 0.0);
	//vec2 sunPos = (vec2(SunXL, SunYL) - 0.5) * 2.0;
	
	// Use the following then SunXL, SunYL is in desktop-resolution coordinates:
	//vec2 sunPos = (2.0 * vec2(SunXL, SunYL) - iResolution.xy) / min(iResolution.x, iResolution.y);
	
	// Use the following when SunXL,YL is a light vector:
	//vec2 sunPos = -2.35 * vec2(-SunXL.x, SunYL);
	//vec2 sunPos = debugFOV * vec2(-SunXL.x, SunYL);

	vec2 sunPos = 0.0;

	// DEBUG
	/*
	float3 col = float3(0.2, 1.0, 0.2); // Reticle color
	d = sdCircle(v.xy, sunPos, scale * cursor_radius);
	dm = smoothstep(thickness, 0.0, abs(d)); // Outer ring
	dm += smoothstep(thickness, 0.0, abs(d + scale * (cursor_radius - 0.001))); // Center dot

	dm = clamp(dm, 0.0, 1.0);
	col *= dm;
	output.color.rgb = lerp(output.color.rgb, col, 0.8 * dm);
	*/

	//float flare = lensflare(v.xy, sunPos), 0.1, 0.0);
	float flare = lensflare(v.xy, sunPos, 0.2 * sun_intensity, 0.0);
	output.color.rgb = lerp(output.color.rgb, flare, 0.8 * flare);
	return output;
}
