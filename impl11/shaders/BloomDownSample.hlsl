#include "./bloom/BloomCommon.fxh"

// Based on the implementation from:
// https://learnopengl.com/Advanced-Lighting/Bloom
// Also look here:
// http://rastergrid.com/blog/2010/09/efficient-gaussian-blur-with-linear-sampling/
// Kernel calculator:
// http://dev.theomader.com/gaussian-kernel-calculator/

Texture2D texture0 : register(t0);
SamplerState sampler0 : register(s0);

cbuffer ConstantBuffer : register(b2)
{
	float pixelSizeX, pixelSizeY, unused1, amplifyFactor;
	// 16 bytes
	float bloomStrength, uvStepSize, saturationStrength, unused2;
	// 32 bytes
};

struct PixelShaderInput
{
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD;
};

float4 main(PixelShaderInput input) : SV_TARGET
{
	float2 input_uv = input.uv * amplifyFactor;
	float2 kernel_small_offsets = uvStepSize * float2(pixelSizeX, pixelSizeY);
	float3 kernel_center = 0.2 * texture0.Sample(sampler0, input_uv).rgb;
	float3 kernel_small = 0;
	float2 offset_uv = 0;

	offset_uv.xy  = input_uv   + kernel_small_offsets;
	kernel_small += 0.75 * 0.2 * texture0.Sample(sampler0, offset_uv).rgb; //++
	offset_uv.x   = input_uv.x - kernel_small_offsets.x;
	kernel_small += 0.2 * texture0.Sample(sampler0, offset_uv).rgb; //-+
	offset_uv.y   = input_uv.y - kernel_small_offsets.y;
	kernel_small += 0.75 * 0.2 * texture0.Sample(sampler0, offset_uv).rgb; //--
	offset_uv.x	  = input_uv.x + kernel_small_offsets.x;
	kernel_small += 0.2 * texture0.Sample(sampler0, offset_uv).rgb; //+-

	return float4(kernel_center + kernel_small, 1);
}
