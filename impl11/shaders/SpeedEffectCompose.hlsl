// Copyright Leo Reyes, 2019.
// Licensed under the MIT license. See LICENSE.txt
// This shader draws the cockpit on top of the blurred background during
// post-hyper-exit
#include "HSV.h"
#include "ShaderToyDefs.h"
#include "ShadertoyCBuffer.h"

// The offscreenBuffer up to this point
Texture2D    colorTex     : register(t0);
SamplerState colorSampler : register(s0);

// The effects (trails) buffer up to this point
Texture2D    effectTex     : register(t1);
SamplerState effectSampler : register(s1);

// The pos3D buffer
Texture2D    posTex     : register(t2);
SamplerState posSampler : register(s2);

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

	float4 texColor = colorTex.Sample(colorSampler, input.uv);
	float4 effectColor = effectTex.Sample(effectSampler, input.uv);
	float3 pos3D = posTex.Sample(posSampler, input.uv).xyz;

	// Initialize the result:
	output.color = texColor;

	// Early exit: avoid adding speed trails inside the cockpit
	if (pos3D.z < 60.0)
		return output;

	//float lightness = dot(0.333, effectColor.rgb);
	// We expect particles to be gray, so it doesn't matter which channel we sample
	float lightness = effectColor.r;
	// Blend the trails with the background
	//float3 color = lerp(texColor.rgb, effectColor.rgb, lightness);
	float3 color = saturate(texColor.rgb + effectColor.rgb);
	output.color = float4(color, 1.0);
	return output;
}
