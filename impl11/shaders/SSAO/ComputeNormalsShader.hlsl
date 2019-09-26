// Copyright (c) 2014 Jérémy Ansel
// Licensed under the MIT license. See LICENSE.txt
// Extended for VR by Leo Reyes (c) 2019

// The 3D position buffer (linear X,Y,Z)
Texture2D    texture0 : register(t0);
SamplerState sampler0 : register(s0);

//static float METRIC_SCALE_FACTOR = 25.0;

struct PixelShaderInput
{
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD;
};

struct PixelShaderOutput
{
	float4 color  : SV_TARGET0; // For debugging purposes, remove later
	float4 normal : SV_TARGET1;
};

cbuffer ConstantBuffer : register(b2)
{
	float pixelSizeX, pixelSizeY, unused1, amplifyFactor;
	// 16 bytes
	float bloomStrength, uvStepSize, saturationStrength, unused2;
	// 32 bytes
};

PixelShaderOutput main(PixelShaderInput input)
{
	PixelShaderOutput output;
	float2 dx = uvStepSize * float2(pixelSizeX, 0);
	float2 dy = uvStepSize * float2(0, pixelSizeY);

	float3 P  = texture0.Sample(sampler0, input.uv).xyz;
	float3 Px = texture0.Sample(sampler0, input.uv + dx).xyz;
	float3 Py = texture0.Sample(sampler0, input.uv + dy).xyz;
	
	//float Z = P.z / (1.0 + P.z);
	//Z = 1.0 - Z;

	//float3 N = normalize(cross(ddx(P), ddy(P)));
	//output.normal = float4(N, 1);

	float3 N = normalize(cross(P - Px, P - Py));
	output.normal = float4(N, 1);
	
	// DEBUG Visualize the normal map:
	//output.color = float4(N.xy, -N.z, 1);
	output.color = float4(float3(N.xy, -N.z) * 0.5 + 0.5, 1);
	//output.color = float4(X,X,X, 1);
	// DEBUG
	return output;
}
