// Copyright (c) 2019 Leo Reyes
// Licensed under the MIT license. See LICENSE.txt
// This shader is currently used to render the VR keyboard and gloves.
//#include "HSV.h"
#include "shader_common.h"
#include "shading_system.h"
#include "PixelShaderTextureCommon.h"

Texture2D texture0 : register(t0);
SamplerState sampler0 : register(s0);

//Texture2D texture1 : register(t1);
//SamplerState sampler1 : register(s1);

// When the Dynamic Cockpit is active:
// texture0 == regular texture
// texture1 == ??? illumination texture?

// New PixelShaderInput needed for the D3DRendererHook
struct PixelShaderInput
{
	float4 pos    : SV_POSITION;
	float4 pos3D  : COLOR1;
	float4 normal : NORMAL;
	float2 tex    : TEXCOORD;
	//float4 color  : COLOR0;
};

struct PixelShaderOutput
{
	float4 color    : SV_TARGET0;
	float4 bloom    : SV_TARGET1;
	float4 pos3D    : SV_TARGET2;
	float4 normal   : SV_TARGET3;
	float4 ssaoMask : SV_TARGET4;
	float4 ssMask   : SV_TARGET5;
};

PixelShaderOutput main(PixelShaderInput input)
{
	PixelShaderOutput output;

	const float4 texelColor = texture0.Sample(sampler0, input.tex);
	const float  alpha      = texelColor.w;
	if (alpha < 0.75)
		discard;

	output.color = texelColor;
	//output.color = float4(N, texelColor.a);

	// Zero-out the bloom mask.
	output.bloom = float4(0, 0, 0, 0);

	float3 P = input.pos3D.xyz;
	output.pos3D = float4(P, 1);

	float3 N = normalize(input.normal.xyz);
	N.y = -N.y; // Invert the Y axis, originally Y+ is down
	N.z = -N.z;
	output.normal = float4(N, 1);

	//output.ssaoMask = 0;
	//output.ssaoMask = float4(fSSAOMaskVal, fGlossiness, fSpecInt, alpha);
	output.ssaoMask = float4(fSSAOMaskVal, 0.1, 0, alpha);
	//output.diffuse = input.color;

	// SS Mask: Normal Mapping Intensity (overriden), Specular Value, Shadeless
	//output.ssMask = float4(fNMIntensity, fSpecVal, 0.0, 0.0);
	output.ssMask = float4(0, 0, 0, 0);

	return output;
}
