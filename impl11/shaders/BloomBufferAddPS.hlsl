//#include "BloomCommon.fxh"
// https://github.com/martymcmodding/qUINT/blob/master/Shaders/qUINT_bloom.fx

// Based on the implementation from:
// https://learnopengl.com/Advanced-Lighting/Bloom

// This texture slot should be the source bloom buffer
Texture2D bloomTex0 : register(t0);
SamplerState sampler0 : register(s0);

// This texture slot should be a copy of the accummulator
Texture2D bloomSumTex : register(t1);
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
};

float4 main(PixelShaderInput input) : SV_TARGET
{
	float2 input_uv_sub = input.uv * amplifyFactor;
	float4 bloom = float4(bloomTex0.Sample(sampler0, input_uv_sub).rgb, 1);
	float4 bloomSum = float4(bloomSumTex.Sample(bloomSumSampler, input.uv).rgb, 1);

	// Truncate negative values coming from the bloom texture:
	return bloomSum + max(0, bloomStrength * bloom);
}