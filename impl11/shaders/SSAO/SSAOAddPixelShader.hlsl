/*
 * Simple shader to add SSAO + Bloom-Mask + Color Buffer
 * Copyright 2019, Leo Reyes.
 * Licensed under the MIT license. See LICENSE.txt
 *
 * The bloom-mask is *not* the final bloom buffer -- this mask is output by the pixel
 * shader and it will be used later to compute proper bloom. Here we use this mask to
 * disable areas of the SSAO buffer that should be bright.
 */
#include "..\shader_common.h"
#include "..\HSV.h"

 // The color buffer
Texture2D texture0 : register(t0);
SamplerState sampler0 : register(s0);

// The bloom mask buffer
Texture2D texBloom : register(t1);
SamplerState samplerBloom : register(s1);

// The SSAO buffer
Texture2D texSSAO : register(t2);
SamplerState samplerSSAO : register(s2);

// The SSAO mask
Texture2D texSSAOMask : register(t3);
SamplerState samplerSSAOMask : register(s3);

// The Normals buffer
Texture2D texNormal : register(t4);
SamplerState samplerNormal : register(s4);

// The Foreground 3D position buffer (linear X,Y,Z)
Texture2D    texPos   : register(t5);
SamplerState sampPos  : register(s5);

// The Background 3D position buffer (linear X,Y,Z)
Texture2D    texPos2  : register(t6);
SamplerState sampPos2 : register(s6);

// We're reusing the same constant buffer used to blur bloom; but here
// we really only use the amplifyFactor to upscale the SSAO buffer (if
// it was rendered at half the resolution, for instance)
cbuffer ConstantBuffer : register(b2)
{
	float pixelSizeX, pixelSizeY, white_point, amplifyFactor;
	// 16 bytes
	float bloomStrength, uvStepSize, saturationStrength;
	uint enableSSAO;
	// 32 bytes
	uint enableBentNormals;
	float norm_weight, unused2, unused3;
};

// SSAOPixelShaderCBuffer
cbuffer ConstantBuffer : register(b3)
{
	float screenSizeX, screenSizeY, indirect_intensity, bias;
	// 16 bytes
	float intensity, near_sample_radius, black_level;
	uint samples;
	// 32 bytes
	uint z_division;
	float bentNormalInit, max_dist, power;
	// 48 bytes
	uint debug;
	float moire_offset, ssao_amplifyFactor;
	uint fn_enable;
	// 64 bytes
	float fn_max_xymult, fn_scale, fn_sharpness, nm_intensity_near;
	// 80 bytes
	float far_sample_radius, nm_intensity_far, ssao_unused2, ssao_unused3;
	// 96 bytes
};

cbuffer ConstantBuffer : register(b4)
{
	matrix projEyeMatrix;
	matrix viewMatrix;
	matrix fullViewMatrix;
};

struct PixelShaderInput
{
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD;
};

float3 getPositionFG(in float2 uv, in float level) {
	// The use of SampleLevel fixes the following error:
	// warning X3595: gradient instruction used in a loop with varying iteration
	// This happens because the texture is sampled within an if statement (if FGFlag then...)
	return texPos.SampleLevel(sampPos, uv, level).xyz;
}

float3 getPositionBG(in float2 uv, in float level) {
	return texPos2.SampleLevel(sampPos2, uv, level).xyz;
}

/*
 * From Pascal Gilcher's SSR shader.
 * https://github.com/martymcmodding/qUINT/blob/master/Shaders/qUINT_ssr.fx
 * (Used with permission from the author)
 */
float3 get_normal_from_color(float2 uv, float2 offset)
{
	float3 offset_swiz = float3(offset.xy, 0);
	// Luminosity samples
	float hpx = dot(texture0.SampleLevel(sampler0, float2(uv + offset_swiz.xz), 0).xyz, 0.333) * fn_scale;
	float hmx = dot(texture0.SampleLevel(sampler0, float2(uv - offset_swiz.xz), 0).xyz, 0.333) * fn_scale;
	float hpy = dot(texture0.SampleLevel(sampler0, float2(uv + offset_swiz.zy), 0).xyz, 0.333) * fn_scale;
	float hmy = dot(texture0.SampleLevel(sampler0, float2(uv - offset_swiz.zy), 0).xyz, 0.333) * fn_scale;

	// Depth samples
	float dpx = getPositionFG(uv + offset_swiz.xz, 0).z;
	float dmx = getPositionFG(uv - offset_swiz.xz, 0).z;
	float dpy = getPositionFG(uv + offset_swiz.zy, 0).z;
	float dmy = getPositionFG(uv - offset_swiz.zy, 0).z;

	// Depth differences in the x and y axes
	float2 xymult = float2(abs(dmx - dpx), abs(dmy - dpy)) * fn_sharpness;
	//xymult = saturate(1.0 - xymult);
	xymult = saturate(fn_max_xymult - xymult);

	float3 normal;
	normal.xy = float2(hmx - hpx, hmy - hpy) * xymult / offset.xy * 0.5;
	normal.z = 1.0;

	return normalize(normal);
}

float3 blend_normals(float3 n1, float3 n2)
{
	n1 += float3(0, 0, 1);
	n2 *= float3(-1, -1, 1);
	return n1 * dot(n1, n2) / n1.z - n2;
}

float4 main(PixelShaderInput input) : SV_TARGET
{
	float2 input_uv_sub = input.uv * amplifyFactor;
	float3 color = texture0.Sample(sampler0, input.uv).xyz;
	float4 bloom = texBloom.Sample(samplerBloom, input.uv);
	float3 ssao = texSSAO.Sample(samplerSSAO, input_uv_sub).rgb;
	float3 ssaoMask = texSSAOMask.Sample(samplerSSAOMask, input.uv).xyz;
	float  mask = max(dot(0.333, bloom.xyz), dot(0.333, ssaoMask));

	if (fn_enable) {
		bool FGFlag;
		float3 n = texNormal.Sample(samplerNormal, input.uv).xyz;
		float3 P1 = getPositionFG(input.uv, 0);
		float3 P2 = getPositionBG(input.uv, 0);
		float3 p;
		if (P1.z < P2.z) {
			p = P1;
			FGFlag = true;
		}
		else {
			p = P2;
			FGFlag = false;
		}
		if (p.z < INFINITY_Z1) {
			float2 offset = float2(pixelSizeX, pixelSizeY);
			float nm_intensity = lerp(nm_intensity_near, nm_intensity_far, saturate(p.z / 4000.0));
			float3 FakeNormal = get_normal_from_color(input.uv, offset);
			n = blend_normals(nm_intensity * FakeNormal, n);
			ssao *= (1 - n.z);
		}
	}

	ssao = lerp(ssao, 1, mask);
	if (debug)
		return float4(ssao, 1);
	else
		return float4(color * ssao, 1);
}
