// Copyright (c) 2014 Jérémy Ansel
// Licensed under the MIT license. See LICENSE.txt
// Extended for VR by Leo Reyes (c) 2019

// The 3D position buffer (linear X,Y,Z)
Texture2D    texture0 : register(t0);
SamplerState sampler0 : register(s0);

// The colorbuf ONLY FOR DEBUG PURPOSES, REMOVE LATER!
Texture2D    texture1 : register(t1);
SamplerState sampler1 : register(s1);

struct PixelShaderInput
{
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD;
};

struct PixelShaderOutput
{
	//float4 color  : SV_TARGET0; // For debugging purposes, remove later
	//float4 normal : SV_TARGET1;
	float4 normal : SV_TARGET0;
};

cbuffer ConstantBuffer : register(b2)
{
	float pixelSizeX, pixelSizeY, unused1, amplifyFactor;
	// 16 bytes
	float bloomStrength, uvStepSize, saturationStrength, unused2;
	// 32 bytes
};

/*
float2 getRandom(in float2 uv)
{
	// g_screen_size is the size of the screen in pixels
	// random_size is the size of the random texture (64x64)
	return normalize(tex2D(g_random, g_screen_size * uv / random_size).xy * 2.0f - 1.0f);
}
*/

/*
inline float2 getRandom(in float2 uv) {
	return normalize(texture1.Sample(sampler0, float2(3280, 2160) * uv / float2(64, 64)).xy * 2.0f - 1.0f);
}
*/

PixelShaderOutput main(PixelShaderInput input)
{
	PixelShaderOutput output;
	//float2 dx = uvStepSize * float2(pixelSizeX, 0);
	//float2 dy = uvStepSize * float2(0, pixelSizeY);

	float3 P  = texture0.Sample(sampler0, input.uv).xyz;
	//float3 Px = texture0.Sample(sampler0, input.uv + dx).xyz;
	//float3 Py = texture0.Sample(sampler0, input.uv + dy).xyz;
	
	float3 N = normalize(cross(ddx(P), ddy(P)));
	output.normal = float4(N, 1);

	//float3 N = normalize(cross(P - Px, P - Py));
	//output.normal = float4(N, 1);
	
	// DEBUG Visualize the normal map:
	//output.color = float4(N.xy, -N.z, 1);
	//output.color = float4(float3(N.xy, -N.z) * 0.5 + 0.5, 1);
	//output.color = float4(getRandom(input.uv), 0, 1);
	// DEBUG
	return output;
}
