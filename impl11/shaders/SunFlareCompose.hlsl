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

PixelShaderOutput main(PixelShaderInput input)
{
	PixelShaderOutput output;

	float4 color = colorTex.Sample(colorSampler, input.uv);
	float4 flare = flareTex.Sample(flareSampler, input.uv);
	float2 sunPos;
	float3 sunPos3D;

	output.color = color;
	if (VRmode == 1)
		sunPos = input.uv.x <= 0.5 ? SunCoords[0].xy * float2(0.5, 1.0) : SunCoords[0].xy * float2(0.5, 1.0) + float2(0.5, 0.0);
	else
		sunPos = SunCoords[0].xy;
	//sunPos.y -= y_center;

	//output.color = float4(input.uv, 0, 1);
	//return output;

	sunPos3D = depthTex.Sample(depthSampler, sunPos).xyz;
	
	/*
	if (sunPos3D.z < INFINITY_Z) 
		return output;

	output.color.rgb += flare.rgb; // Original compose
	*/

	// DEBUG

	// Display the depth buffer to confirm it's properly aligned:
	//sunPos3D = depthTex.Sample(depthSampler, input.uv).xyz;
	//if (sunPos3D.z < INFINITY_Z)
	//	output.color = float4(0, 1, 0, 1);
	//return output;

	if (sunPos3D.z < INFINITY_Z) {
		float d = 3.0 * dot(flare.rgb, 0.333);
		if (d > 0.1)
			flare.rgb = float3(0, 1, 0);
		output.color.rgb = lerp(output.color.rgb, flare.rgb, d);
		return output;
	}

	float d = 3.0 * dot(flare.rgb, 0.333);
	output.color.rgb = lerp(output.color.rgb, flare.rgb, d);
	// DEBUG
	return output;
}
