// Copyright Leo Reyes, 2019.
// Licensed under the MIT license. See LICENSE.txt
// This shader draws the cockpit on top of the blurred background during
// post-hyper-exit
#include "HSV.h"

// The foreground texture (shadertoyBuf)
Texture2D    texture0 : register(t0);
SamplerState sampler0 : register(s0);

// The background texture (shadertoyAuxBuf)
Texture2D    texture1 : register(t1);
SamplerState sampler1 : register(s1);

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
	float4 fgColor = texture0.Sample(sampler0, input.uv);
	float4 bgColor = texture1.Sample(sampler1, input.uv);
	
	output.color.rgb = lerp(bgColor.rgb, fgColor.rgb, fgColor.w);
	output.color.a = 1.0;
	return output;
}
