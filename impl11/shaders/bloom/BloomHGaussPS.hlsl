#include "BloomCommon.fxh"

// Based on the implementation from:
// https://learnopengl.com/Advanced-Lighting/Bloom
// Also look here:
// http://rastergrid.com/blog/2010/09/efficient-gaussian-blur-with-linear-sampling/
// Kernel calculator:
// http://dev.theomader.com/gaussian-kernel-calculator/

Texture2D texture0 : register(t0);
SamplerState sampler0 : register(s0);

cbuffer ConstantBuffer : register(b1)
{
	float pixelSizeX, pixelSizeY, colorMul, alphaMul;
	// 16 bytes
	float amplifyFactor, unused1, unused, unused3;
	// 32 bytes
};

struct PixelShaderInput
{
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD;
};

float4 main(PixelShaderInput input) : SV_TARGET
{
	float2 input_uv = input.uv;
	//float4 color0 = texture0.Sample(sampler0, input_uv);
	float3 color0 = texture0.Sample(sampler0, input_uv).xyz;
	float3 color = color0 * weight[0];
	float3 s1, s2;
	float2 dx = float2(pixelSizeX, 0);
	float2 uv1 = input_uv + dx;
	float2 uv2 = input_uv - dx;

	[unroll]
	for (int i = 1; i < 5; i++) {
		s1 = texture0.Sample(sampler0, uv1).xyz * weight[i];
		s2 = texture0.Sample(sampler0, uv2).xyz * weight[i];
		color.xyz += s1 + s2;
		uv1 += dx;
		uv2 -= dx;
	}
	//color.w *= alphaMul;
	return float4(colorMul * color, 1);
}