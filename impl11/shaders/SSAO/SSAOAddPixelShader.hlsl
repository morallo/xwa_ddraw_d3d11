/*
 * Simple Addition for SSAO + colorbuf
 */
// The color buffer
Texture2D texture0 : register(t0);
SamplerState sampler0 : register(s0);

// The bloom buffer (I believe it's scaled 1:1 wrt the color buffer)
//Texture2D texBloom : register(t1);
//SamplerState samplerBloom : register(s1);

// The SSAO buffer
Texture2D texSSAO : register(t2);
SamplerState samplerSSAO : register(s2);

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
	float3 color = texture0.Sample(sampler0, input.uv);
	//float3 bloom = texBloom.Sample(samplerBloom, input.uv);
	float ssao = texSSAO.Sample(samplerSSAO, input_uv_sub).r;
	
	//return float4(color, 1);
	return float4(color * ssao, 1);
	//return float4(ssao,ssao,ssao, 1);
}