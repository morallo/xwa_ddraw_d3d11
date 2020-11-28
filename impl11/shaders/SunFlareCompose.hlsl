// Copyright Leo Reyes, 2019.
// Licensed under the MIT license. See LICENSE.txt
// This shader draws the cockpit on top of the blurred background during
// post-hyper-exit
#include "HSV.h"
#include "ShaderToyDefs.h"
#include "ShadertoyCBuffer.h"

// The color texture
Texture2D    colorTex     : register(t0);
SamplerState colorSampler : register(s0);

// The flare texture
Texture2D    flareTex     : register(t1);
SamplerState flareSampler : register(s1);

// The depth texture (shadertoyAuxBuf)
Texture2D    depthTex     : register(t2);
SamplerState depthSampler : register(s2);

#define INFINITY_Z 30000.0

struct PixelShaderInput
{
	float4 pos : SV_POSITION;
	float2 uv  : TEXCOORD;
};

struct PixelShaderOutput
{
	float4 color    : SV_TARGET0;
};

/*
float sdCircle(in vec2 p, float radius)
{
	return length(p) - radius;
}
*/

PixelShaderOutput main(PixelShaderInput input)
{
	PixelShaderOutput output;

	float4 color = colorTex.Sample(colorSampler, input.uv);
	float4 flare = flareTex.Sample(flareSampler, input.uv);
	float2 sunPos;
	float3 sunPos3D;
	//float2 debug_uv;

	output.color = color;
	if (VRmode == 1) {
		sunPos = input.uv.x <= 0.5 ? SunCoords[0].xy * float2(0.5, 1.0) : SunCoords[0].xy * float2(0.5, 1.0) + float2(0.5, 0.0);
		//debug_uv = input.uv.x <= 0.5 ? input.uv.xy * float2(2.0, 1.0) : input.uv.xy * float2(2.0, 1.0) - float2(1.0, 0.0);
	}
	else {
		sunPos = SunCoords[0].xy;
		//debug_uv = input.uv.xy;
	}

	//output.color = float4(input.uv, 0, 1);
	//return output;

	sunPos3D = depthTex.Sample(depthSampler, sunPos).xyz;
	
	if (sunPos3D.z < INFINITY_Z) 
		return output;

	output.color.rgb += flare.rgb; // Original compose

	// DEBUG
	/*
	// Display the depth buffer to confirm it's properly aligned:
	//sunPos3D = depthTex.Sample(depthSampler, input.uv).xyz;
	//if (sunPos3D.z < INFINITY_Z)
	//	output.color = float4(0, 1, 0, 1);
	//return output;

	float d, dm;
	// Make the marker red if occluded:
	if (sunPos3D.z < INFINITY_Z) {
		d = 3.0 * dot(flare.rgb, 0.333);
		if (d > 0.1)
			flare.rgb = float3(1, 0.1, 0.1);
		output.color.rgb = lerp(output.color.rgb, flare.rgb, d);
		return output;
	}

	d = 3.0 * dot(flare.rgb, 0.333);
	output.color.rgb = lerp(output.color.rgb, flare.rgb, d);
	if (any(debug_uv > 0.5))
		output.color.g += 0.15;


	// The following produces a couple of very round circles in the middle of each
	// SBS screen, regardless of window size
	float thickness = 0.02, scale = 2.0, cursor_radius = 0.04;
	//vec2 fragCoord = input.uv * iResolution.xy;
	vec2 fragCoord = debug_uv * iResolution.xy;
	vec2 p = (2.0 * fragCoord.xy - iResolution.xy) / min(iResolution.x, iResolution.y);
	float3 col = float3(1.0, 1.0, 0.1); // Marker color
	d = sdCircle(float2(0.5, 1.0) * p.xy, 0.05);
	dm = smoothstep(thickness * 0.5, 0.0, abs(d)); // Outer ring
	dm += smoothstep(thickness * 0.5, 0.0, abs(d + 0.5 * scale * (cursor_radius - 0.001))); // Center dot
	col *= dm;
	output.color.rgb = lerp(output.color.rgb, col, 0.8 * dm);
	*/
	// DEBUG
	return output;
}
