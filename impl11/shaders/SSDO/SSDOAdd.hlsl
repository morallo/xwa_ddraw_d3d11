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

// The SSDO Direct buffer
Texture2D texSSDO : register(t2);
SamplerState samplerSSDO : register(s2);

// The SSDO Indirect buffer
Texture2D texSSDOInd : register(t3);
SamplerState samplerSSDOInd : register(s3);

// The SSAO mask
Texture2D texSSAOMask : register(t4);
SamplerState samplerSSAOMask : register(s4);

// The position/depth buffer
Texture2D texPos : register(t5);
SamplerState sampPos : register(s5);

// The (Flat) Normals buffer
Texture2D texNormal : register(t6);
SamplerState samplerNormal : register(s6);

#define INFINITY_Z 20000

// We're reusing the same constant buffer used to blur bloom; but here
// we really only use the amplifyFactor to upscale the SSAO buffer (if
// it was rendered at half the resolution, for instance)
// Bloom Constant Buffer
cbuffer ConstantBuffer : register(b2)
{
	float pixelSizeX, pixelSizeY, unused0, amplifyFactor;
	// 16 bytes
	float bloomStrength, uvStepSize, saturationStrength;
	uint unused1;
	// 32 bytes
	uint unused2;
	float unused3, depth_weight;
	uint debug;
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
	uint ssao_debug;
	float moire_offset, ssao_amplifyFactor;
	uint fn_enable;
	// 64 bytes
	float fn_max_xymult, fn_scale, fn_sharpness, nm_intensity_near;
	// 80 bytes
	float far_sample_radius, nm_intensity_far, ambient, ssao_amplifyFactor2;
	// 96 bytes
	float x0, y0, x1, y1; // Viewport limits in uv space
	// 112 bytes
	float3 invLightColor;
	float gamma;
	// 128 bytes
	float white_point, shadow_step_size, shadow_steps, aspect_ratio;
	// 144 bytes
	float4 vpScale;
	// 160 bytes
	uint shadow_enable;
	float shadow_k, ssao_unused1, ssao_unused2;
	// 176 bytes
};

cbuffer ConstantBuffer : register(b4)
{
	float4 LightVector;
	float4 LightColor;
	float4 LightVector2;
	float4 LightColor2;
};

struct PixelShaderInput
{
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD;
};

// (Sorry, don't remember very well) I think this function projects a 3D point back
// into 2D and then converts the 2D coord into its equivalent UV-coord used in post
// processing. This function is going to be useful to project 3D into 2D when trying
// to make iteractions with controllers in VR or just when adding extra geometry to
// the game later on.
inline float2 projectToUV(in float3 pos3D) {
	float3 P = pos3D;
	float w = P.z / METRIC_SCALE_FACTOR;
	P.xy = P.xy / P.z;
	// Convert to vertex pos:
	//P.xy = ((P.xy / (vpScale.z * float2(aspect_ratio, 1))) - float2(-0.5, 0.5)) / vpScale.xy;
	// (-1,-1)-(1, 1)
	P.xy /= (vpScale.z * float2(aspect_ratio, 1));
	P.xy -= float2(-0.5, 0.5);
	P.xy /= vpScale.xy;
	// We now have P = input.pos
	P.x = (P.x * vpScale.x - 1.0f) * vpScale.z;
	P.y = (P.y * vpScale.y + 1.0f) * vpScale.z;
	//P *= 1.0f / w; // Don't know if this is 100% necessary... probably not

	// Now convert to UV coords: (0, 1)-(1, 0):
	//P.xy = lerp(float2(0, 1), float2(1, 0), (P.xy + 1) / 2);
	// The viewport used to render the original offscreenBuffer may not cover the full
	// screen, so the uv coords have to be adjusted to the limits of the viewport within
	// the full-screen quad:
	P.xy = lerp(float2(x0, y1), float2(x1, y0), (P.xy + 1) / 2);
	return P.xy;
}

/*
float3 shadow_factor(in float3 P) {
	float3 cur_pos = P, occluder, diff;
	float2 cur_uv;
	float3 ray_step = shadow_step_size * LightVector.xyz;
	int steps = (int)shadow_steps;
	float shadow_length = shadow_step_size * shadow_steps;
	float cur_length = 0;
	float res = 1.0;

	// Handle samples that land outside the bounds of the image
	// "negative" cur_diff should be ignored
	[loop]
	for (int i = 1; i <= steps; i++) {
		cur_pos += ray_step;
		cur_length += shadow_step_size;
		cur_uv = projectToUV(cur_pos);

		// If the ray has exited the current viewport, we're done:
		if (cur_uv.x < x0 || cur_uv.x > x1 ||
			cur_uv.y < y0 || cur_uv.y > y1)
			return float3(res, cur_length / shadow_length, 1);

		occluder = texPos.SampleLevel(sampPos, cur_uv, 0).xyz;
		diff = occluder - cur_pos;
		if (diff.z > 0) { // Ignore negative z-diffs: the occluder is behind the ray
			//if (diff.z < 0.01)
			//	return float3(0, cur_length / shadow_length, 0);
			res = min(res, saturate(shadow_k * diff.z / (cur_length + 0.00001)));
		}

		//cur_pos += ray_step;
		//cur_length += shadow_step_size;
	}
	return float3(res, cur_length / shadow_length, 0);
}
*/

float4 main(PixelShaderInput input) : SV_TARGET
{
	float2 input_uv_sub = input.uv * amplifyFactor;
	//float2 input_uv_sub2 = input.uv * amplifyFactor2;
	float2 input_uv_sub2 = input.uv * amplifyFactor;
	float3 color    = texture0.Sample(sampler0, input.uv).xyz;
	//float3 bentN  = texBent.Sample(samplerBent, input_uv_sub).xyz;
	//float3 Normal = texNormal.Sample(samplerNormal, input.uv).xyz;
	float3 pos3D		= texPos.Sample(sampPos, input.uv).xyz;
	float4 bloom    = texBloom.Sample(samplerBloom, input.uv);
	float3 ssdo     = texSSDO.Sample(samplerSSDO, input_uv_sub).rgb;
	float3 ssdoInd  = texSSDOInd.Sample(samplerSSDOInd, input_uv_sub2).rgb;
	float3 ssaoMask = texSSAOMask.Sample(samplerSSAOMask, input.uv).xyz;
	//float  mask = max(dot(0.333, bloom.xyz), dot(0.333, ssaoMask));
	float mask = dot(0.333, ssaoMask);
	
	if (pos3D.z > INFINITY_Z || mask > 0.9)
		return float4(color, 1);

	/*
	// Compute shadows
	float m_offset = max(moire_offset, moire_offset * (pos3D.z * 0.1));
	//pos3D.z -= m_offset;
	pos3D += Normal * m_offset;
	float3 shadow = shadow_factor(pos3D);
	shadow = shadow.xxx;
	if (!shadow_enable)
		shadow = 1;
	*/

	ssdo = ambient + ssdo; // * shadow; // Add the ambient component 
	ssdo = lerp(ssdo, 1, mask);
	ssdoInd = lerp(ssdoInd, 0, mask);

	//if (debug == 4)
	//	return float4(ssdoInd, 1);
	return float4(pow(abs(color), 1/gamma) * ssdo + ssdoInd, 1);
	//return float4(color * ssdo, 1);
	
	//color = saturate((ambient + diffuse) * color);
	//ssao = enableSSAO ? ssao : 1.0f;

	//return float4(ssdo, 1);

	//return float4(color * ssao, 1);
	//return float4(saturate(color * ssao + bentN * light_factor), 1);
	//return float4(ssao, 1);

	// Let's use SSAO to also lighten some areas:
	//float3 screen_layer = 1 - (1 - color) * (1 - ssao * white_point);
	//float3 mix = lerp(mult_layer, screen_layer, ssao.r);
	//return float4(mix, 1);

	//float3 HSV = RGBtoHSV(color);
	//HSV.z *= ssao.r;
	//HSV.z = ssao.r;
	//color = HSVtoRGB(HSV);
	//return float4(color, 1);
}
