/*
 * Simple shader to compute the normal buffer from the position buffer
 * Copyright 2019, Leo Reyes.
 * Licensed under the MIT license. See LICENSE.txt
 */

// The 3D position buffer (linear X,Y,Z)
Texture2D    DepthTex : register(t0);
SamplerState DepthSampler : register(s0);

// The Normal Buffer
Texture2D    NormTex : register(t1);
SamplerState NormSampler : register(s1);

struct PixelShaderInput
{
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD;
};

struct PixelShaderOutput
{
	float4 normal : SV_TARGET0;
};

// Re-using the blur pixel shader constant buffer:
cbuffer ConstantBuffer : register(b2)
{
	float pixelSizeX, pixelSizeY, white_point, amplifyFactor;
	// 16 bytes
	float bloomStrength, uvStepSize, saturationStrength;
	uint enableSSAO;
	// 32 bytes
	uint enableBentNormals;
	float unused1, depth_weight, normal_blur_radius;
};

PixelShaderOutput main_old(PixelShaderInput input)
{
	PixelShaderOutput output;
	//float2 dx = uvStepSize * float2(pixelSizeX, 0);
	//float2 dy = uvStepSize * float2(0, pixelSizeY);

	float3 P  = DepthTex.Sample(DepthSampler, input.uv).xyz;
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

void smooth_normals(in float2 input_uv, inout float3 normal, in float3 position)
{
	//float2 scaled_radius = 0.018 / position.z * qUINT::ASPECT_RATIO;
	//float2 scaled_radius = 0.018 / position.z;
	float2 scaled_radius = normal_blur_radius;
	float3 neighbour_normal[4] = { normal, normal, normal, normal };

	[unroll]
	for (int i = 0; i < 4; i++)
	{
		float2 direction;
		sincos(6.28318548 * 0.25 * i, direction.y, direction.x);

		[unroll]
		for (int direction_step = 1; direction_step <= 5; direction_step++)
		{
			float search_radius = exp2(direction_step);
			float2 sample_uv = input_uv + direction * search_radius * scaled_radius;

			float3 temp_normal   = NormTex.Sample(NormSampler, sample_uv).xyz;
			float3 temp_position = DepthTex.Sample(DepthSampler, sample_uv).xyz;

			float3 position_delta = temp_position - position;
			float distance_weight = saturate(1.0 - dot(position_delta, position_delta) * 20.0 / search_radius);
			float normal_angle = dot(normal, temp_normal);
			float angle_weight = smoothstep(0.3, 0.98, normal_angle) * smoothstep(1.0, 0.98, normal_angle); //only take normals into account that are NOT equal to the current normal.
			//float angle_weight = smoothstep(0.3, 0.98, normal_angle);

			//float total_weight = saturate(3.0 * distance_weight * angle_weight / search_radius);
			float total_weight = saturate(3.0 * angle_weight / search_radius);

			neighbour_normal[i] = lerp(neighbour_normal[i], temp_normal, total_weight);
		}
	}

	normal = normalize(neighbour_normal[0] + neighbour_normal[1] + neighbour_normal[2] + neighbour_normal[3]);
}

PixelShaderOutput main(PixelShaderInput input) {
	PixelShaderOutput output;
	float3 normal   = NormTex.Sample(NormSampler, input.uv).xyz;
	float3 position = DepthTex.Sample(DepthSampler, input.uv).xyz;
	smooth_normals(input.uv, normal, position);
	output.normal   = float4(normal * 0.5 + 0.5, 1);
	return output;
}