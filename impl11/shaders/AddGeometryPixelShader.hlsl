/*
 * You can use it under the terms of the MIT license, see LICENSE.TXT
 * (free to use even in commercial projects, attribution required)
 *
 * (c) Leo Reyes, 2020.
 */

#include "ShaderToyDefs.h"
#include "ShadertoyCBuffer.h"

struct PixelShaderInput
{
	float4 pos		: SV_POSITION;
	float4 color	: COLOR0;
	float4 normal	: COLOR1;
	float2 tex		: TEXCOORD;
};

struct PixelShaderOutput
{
	float4 color    : SV_TARGET0;
};

PixelShaderOutput main(PixelShaderInput input) {
	PixelShaderOutput output;
	//output.color = float4(0.0, 0.0, 0.0, 1.0);
	output.color = input.color;
	return output;
}