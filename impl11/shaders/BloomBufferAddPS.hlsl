//#include "BloomCommon.fxh"
// https://github.com/martymcmodding/qUINT/blob/master/Shaders/qUINT_bloom.fx

// Based on the implementation from:
// https://learnopengl.com/Advanced-Lighting/Bloom

// This texture slot should be the source bloom buffer
Texture2DArray bloomTex0 : register(t0);
SamplerState sampler0 : register(s0);

// This texture slot should be a copy of the accummulator
Texture2DArray bloomSumTex : register(t1);
SamplerState bloomSumSampler : register(s1);

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
	uint viewId: SV_RenderTargetArrayIndex;
};

float4 main(PixelShaderInput input) : SV_TARGET
{
	float2 input_uv_sub = input.uv * amplifyFactor;
	float3 bloom = bloomTex0.Sample(sampler0, float3(input_uv_sub, input.viewId)).rgb;
	float3 bloomSum = bloomSumTex.Sample(bloomSumSampler, float3(input.uv, input.viewId)).rgb;

	// Truncate negative values coming from the bloom texture:
	return float4(bloomSum.rgb + max(0, bloomStrength * bloom.rgb), 1);
}