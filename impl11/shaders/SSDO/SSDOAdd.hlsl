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
#include "..\shading_system.h"
#include "..\SSAOPSConstantBuffer.h"
#include "..\shadow_mapping_common.h"

 // The color buffer
Texture2D texColor : register(t0);
SamplerState sampColor : register(s0);

// The bloom mask buffer
//Texture2D texBloom : register(t1);
//SamplerState samplerBloom : register(s1);

// The SSDO Direct buffer
Texture2D texSSDO : register(t1);
SamplerState samplerSSDO : register(s1);

// The SSDO Indirect buffer
Texture2D texSSDOInd : register(t2);
SamplerState samplerSSDOInd : register(s2);

// The SSAO mask
Texture2D texSSAOMask : register(t3);
SamplerState samplerSSAOMask : register(s3);

// The position/depth buffer
Texture2D texPos : register(t4);
SamplerState sampPos : register(s4);

// The (Smooth) Normals buffer
Texture2D texNormal : register(t5);
SamplerState samplerNormal : register(s5);

// The Bent Normals buffer
Texture2D texBent : register(t6);
SamplerState samplerBent : register(s6);

// The Shading System Mask buffer
Texture2D texSSMask : register(t7);
SamplerState samplerSSMask : register(s7);

// The Shadow Map buffer
Texture2DArray<float> texShadowMap : register(t8);
//SamplerState samplerShadowMap : register(s8);
SamplerComparisonState cmpSampler : register(s8);

/*
{
	// sampler state
	//Filter = COMPARISON_MIN_MAG_MIP_LINEAR;
	Filter = COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
	//AddressU = CLAMP;
	//AddressV = CLAMP;

	// sampler comparison state
	//ComparisonFunc = LESS;
	ComparisonFunc = GREATER_THAN;
};
*/


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

struct PixelShaderInput
{
	float4 pos : SV_POSITION;
	float2 uv  : TEXCOORD;
};

struct PixelShaderOutput
{
	float4 color : SV_TARGET0;
	float4 bloom : SV_TARGET1;
	float4 bent  : SV_TARGET2;
};

float3 getPositionFG(in float2 uv, in float level) {
	// The use of SampleLevel fixes the following error:
	// warning X3595: gradient instruction used in a loop with varying iteration
	// This happens because the texture is sampled within an if statement (if FGFlag then...)
	return texPos.SampleLevel(sampPos, uv, level).xyz;
}

/*
 * From Pascal Gilcher's SSR shader.
 * https://github.com/martymcmodding/qUINT/blob/master/Shaders/qUINT_ssr.fx
 * (Used with permission from the author)
 */
float3 get_normal_from_color(in float2 uv, in float2 offset, in float nm_intensity /*, out float diff */)
{
	float3 offset_swiz = float3(offset.xy, 0);
	float nm_scale = fn_scale * nm_intensity;
	// Luminosity samples
	float hpx = dot(texColor.SampleLevel(sampColor, float2(uv + offset_swiz.xz), 0).xyz, 0.333) * nm_scale;
	float hmx = dot(texColor.SampleLevel(sampColor, float2(uv - offset_swiz.xz), 0).xyz, 0.333) * nm_scale;
	float hpy = dot(texColor.SampleLevel(sampColor, float2(uv + offset_swiz.zy), 0).xyz, 0.333) * nm_scale;
	float hmy = dot(texColor.SampleLevel(sampColor, float2(uv - offset_swiz.zy), 0).xyz, 0.333) * nm_scale;

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
	//diff = clamp(1.0 - (abs(normal.x) + abs(normal.y)), 0.0, 1.0);
	////diff *= diff;

	return normalize(normal);
}

// n1: base normal
// n2: detail normal
float3 blend_normals(float3 n1, float3 n2)
{
	// I got this from Pascal Gilcher; but there's more details here:
	// https://blog.selfshadow.com/publications/blending-in-detail/
	//return normalize(float3(n1.xy*n2.z + n2.xy*n1.z, n1.z*n2.z));

	// UDN:
	//return normalize(float3(n1.xy + n2.xy, n1.z));

	n1.z += 1.0;
	n2.xy = -n2.xy;
	return normalize(n1 * dot(n1, n2) - n1.z * n2);
	//return n1 * dot(n1, n2) - n1.z * n2;
}

#ifdef DISABLED
// (Sorry, don't remember very well) I think this function projects a 3D point back
// into 2D and then converts the 2D coord into its equivalent UV-coord used in post
// processing. This function is going to be useful to project 3D into 2D when trying
// to make iteractions with controllers in VR or just when adding extra geometry to
// the game later on.
inline float2 projectToUV(in float3 pos3D) {
	float3 P = pos3D;
	//float w = P.z / (METRIC_SCALE_FACTOR * metric_mult);
	//float w = P.z / METRIC_SCALE_FACTOR;
	P.xy = /* g_fFocalDist * */ P.xy / P.z;
	// Convert to vertex pos:
	//P.xy = ((P.xy / (vpScale.z * float2(aspect_ratio, 1))) - float2(-0.5, 0.5)) / vpScale.xy;
	// (-1,-1)-(1, 1)
	P.xy /= (vpScale.z * float2(aspect_ratio, 1));
	//P.xy -= float2(-0.5, 0.5); // Is this wrong? I think the y axis is inverted, so adding 0.5 would be fine (?)
	P.xy -= float2(-1.0, 1.0); // DirectSBS may be different
	P.xy /= vpScale.xy;
	// We now have P = input.pos
	//P.x = (P.x * vpScale.x - 1.0f) * vpScale.z;
	//P.y = (P.y * vpScale.y + 1.0f) * vpScale.z;
	P.xy = (P.xy * vpScale.xy + float2(-1.0f, 1.0f)) * vpScale.z;

	// Now convert to UV coords: (0, 1)-(1, 0):
	//P.xy = lerp(float2(0, 1), float2(1, 0), (P.xy + 1) / 2);
	// The viewport used to render the original offscreenBuffer may not cover the full
	// screen, so the uv coords have to be adjusted to the limits of the viewport within
	// the full-screen quad:
	//P.xy = lerp(float2(x0, y1), float2(x1, y0), (P.xy + 1.0) / 2.0);
	P.xy = lerp(float2(p0.x, p1.y), float2(p1.x, p0.y), (P.xy + 1.0) / 2.0);
	return P.xy;
}
#endif

/*
float3 shadow_factor(in float3 P, float max_dist_sqr) {
	float3 cur_pos = P, occluder, diff;
	float2 cur_uv;
	float3 ray_step = shadow_step_size * float3(LightVector[0].xy, -LightVector[0].z);
	//float3 ray_step = shadow_step_size * LightVector[0].xyz;
	int steps = (int)shadow_steps;
	float max_shadow_length = shadow_step_size * shadow_steps;
	float max_shadow_length_sqr = max_shadow_length * 0.75; // Fade the shadow a little before it reaches a hard edge
	max_shadow_length_sqr *= max_shadow_length_sqr;
	float cur_length = 0, length_at_res = INFINITY_Z1;
	float res = 1.0;
	float weight = 1.0;
	//float occ_dot;

	// Handle samples that land outside the bounds of the image
	// "negative" cur_diff should be ignored
	[loop]
	for (int i = 1; i <= steps; i++) {
		cur_pos += ray_step;
		cur_length += shadow_step_size;
		cur_uv = projectToUV(cur_pos);

		// If the ray has exited the current viewport, we're done:
		//if (cur_uv.x < x0 || cur_uv.x > x1 ||
		//	cur_uv.y < y0 || cur_uv.y > y1) 
		if (any(cur_uv < p0) ||
			any(cur_uv > p1))
		{
			weight = saturate(1 - length_at_res * length_at_res / max_shadow_length_sqr);
			res = lerp(1, res, weight);
			return float3(res, cur_length / max_shadow_length, 1);
		}

		occluder = texPos.SampleLevel(sampPos, cur_uv, 0).xyz;
		diff = cur_pos - occluder; // ATTENTION: The substraction is inverted wrt to SSDO!
		//v        = normalize(diff);
		//occ_dot  = max(0.0, dot(LightVector.xyz, v) - bias);

		if (diff.z > shadow_epsilon) { // Ignore negative z-diffs: the occluder is behind the ray
			// If diff.z is too large, ignore it. Or rather, fade with distance
			//float weight = saturate(1.0 - (diff.z * diff.z / max_dist_sqr));
			//float dist = saturate(lerp(1, diff.z), weight);
			float cur_res = saturate(shadow_k * diff.z / (cur_length + 0.00001));
			//cur_res = saturate(lerp(1, cur_res, weight)); // Fadeout if diff.z is too big
			if (cur_res < res) {
				res = cur_res;
				length_at_res = cur_length;
			}
		}
	}
	weight = saturate(1 - length_at_res * length_at_res / max_shadow_length_sqr);
	res = lerp(1, res, weight);
	return float3(res, cur_length / max_shadow_length, 0);
}
*/

// From https://www.gamedev.net/tutorials/programming/graphics/effect-area-light-shadows-part-1-pcss-r4971/
/*
 * We expect Q.xy to be 2D coords; but Q.z must be a depth stencil value in the range 0...1
 */
inline float ShadowMapPCF(float idx, float3 Q, float resolution, int filterSize, float radius)
{
	float shadow = 0.0f;
	float2 sm_pos = Q.xy;
	// Convert to texture coords: this maps -1..1 to 0..1:
	sm_pos = lerp(0, 1, sm_pos * float2(0.5, -0.5) + 0.5);
	float2 grad = frac(sm_pos * resolution + 0.5);
	// We assume that Q.z has already been bias-compensated
	float samples = 0.0;
	float2 ofs = 0.0;

	ofs.y = -radius;
	for (int i = -filterSize; i <= filterSize; i++)
	{
		ofs.x = -radius;
		for (int j = -filterSize; j <= filterSize; j++)
		{
			float4 tmp = texShadowMap.Gather(sampPos, //samplerShadowMap,
				float3(sm_pos + ofs, idx));
			// We're comparing depth values here. For OBJ-based shadow maps, 1 is infinity 0 is ZNear
			// so if a dpeth value is *lower* than Q.z it's an occluder
			tmp.x = tmp.x < Q.z ? 0.0f : 1.0f;
			tmp.y = tmp.y < Q.z ? 0.0f : 1.0f;
			tmp.z = tmp.z < Q.z ? 0.0f : 1.0f;
			tmp.w = tmp.w < Q.z ? 0.0f : 1.0f;
			shadow += lerp(lerp(tmp.w, tmp.z, grad.x), lerp(tmp.x, tmp.y, grad.x), grad.y);
			samples++;
			ofs.x += radius;
		}
		ofs.y += radius;
	}

	return shadow / samples;
}

/*
 * We expect Q.xy to be 2D coords; but Q.z must be a depth stencil value in the range 0...1
 */
inline float ShadowMapPCF_HW(float idx, float3 Q, int filterSize, float radius)
{
	float shadow = 0.0f;
	float2 sm_pos = Q.xy;
	// Convert to texture coords: this maps -1..1 to 0..1:
	sm_pos = lerp(0, 1, sm_pos * float2(0.5, -0.5) + 0.5);
	// We assume that Q.z has already been bias-compensated
	float Z = MetricZToDepth(idx, Q.z);
	float samples = 0.0;
	float2 ofs = 0.0;

	ofs.y = -radius;
	for (int i = -filterSize; i <= filterSize; i++) {
		ofs.x = -radius;
		for (int j = -filterSize; j <= filterSize; j++)
		{
			shadow += saturate(texShadowMap.SampleCmpLevelZero(cmpSampler,
				float3(sm_pos + ofs, idx),
				Z));
			samples++;
			ofs.x += radius;
		}
		ofs.y += radius;
	}

	return shadow / samples;
}

// From http://developer.download.nvidia.com/whitepapers/2008/PCSS_Integration.pdf
inline float PenumbraSize(float zReceiver, float zBlocker)
{
	return (zReceiver - zBlocker) / zBlocker;
}

inline void FindBlocker(float idx, float3 Q, out float d_blocker, out float samples) 
{
	d_blocker = 0.0;
	samples = 0.0;

	// Convert to texture coords: this maps -1..1 to 0..1:
	float2 sm_pos = lerp(0, 1, Q.xy * float2(0.5, -0.5) + 0.5);
	float z = MetricZToDepth(idx, Q.z/* + sm_bias*/);
	float2 ofs = 0;

	ofs.y = -sm_blocker_radius;
	for (int i = -1; i <= 1; i++) 
	{
		ofs.x = -sm_blocker_radius;
		for (int j = -1; j <= 1; j++)
		{
			float sm_Z = texShadowMap.SampleLevel(sampPos, //samplerShadowMap,
				float3(sm_pos + ofs, idx), 0).x;
			// Convert the depth-stencil coord (0..1) to metric Z and compare
			if (sm_Z < z) // This sample is a blocker
			{
				d_blocker += sm_Z;
				samples++;
			}
			ofs.x += sm_blocker_radius;
		}
		ofs.y += sm_blocker_radius;
	}

	if (samples > 0.0) {
		d_blocker /= samples;
		d_blocker = DepthToMetricZ(idx, d_blocker);
	}
}

inline float PCSS(float idx, float3 Q)
{
	float samples, zBlocker; // Metric Z
	// Step 1: Blocker search
	FindBlocker(idx, Q, zBlocker, samples);
	if (samples < 1.0)
		return 1.0;

	// Step 2: Penumbra size
	float penumbraRatio = clamp(PenumbraSize(Q.z, zBlocker), 0.0, 5.0);
	//float filterRadiusUV = sm_light_size * penumbraRatio / sm_pcss_samples; // / MetricZToDepth(Q.z);
	float filterRadiusUV = sm_pcss_radius * sm_light_size * penumbraRatio;
	float radius = sm_pcss_radius + filterRadiusUV;
	float filter_samples = sm_pcss_samples + penumbraRatio / 2.0;

	// Step 3: Filtering
	//return ShadowMapPCF(float3(Q.xy, MetricZToDepth(Q.z + sm_bias)), sm_resolution, sm_pcss_samples, filterRadiusUV);
	return ShadowMapPCF(idx, float3(Q.xy, MetricZToDepth(idx, Q.z/* + sm_bias*/)), sm_resolution, filter_samples, radius);
	//return ShadowMapPCF(float3(Q.xy, MetricZToDepth(Q.z + sm_bias)), sm_resolution, sm_pcss_samples, penumbraRatio * sm_light_size * sm_pcss_radius);
}

inline float get_black_level(uint idx)
{
	return sm_black_levels[idx >> 2][idx & 0x03];
}

PixelShaderOutput main(PixelShaderInput input)
{
	PixelShaderOutput output;
	output.color = 0;
	output.bloom = 0;
	output.bent  = 0;

	float2 input_uv_sub  = input.uv * amplifyFactor;
	//float2 input_uv_sub2 = input.uv * amplifyFactor2;
	float2 input_uv_sub2 = input.uv * amplifyFactor;
	float3 color         = texColor.Sample(sampColor, input.uv).xyz;
	float4 Normal        = texNormal.Sample(samplerNormal, input.uv);
	float3 pos3D		 = texPos.Sample(sampPos, input.uv).xyz;
	float3 ssdo          = texSSDO.Sample(samplerSSDO, input_uv_sub).rgb;
	float3 ssdoInd       = texSSDOInd.Sample(samplerSSDOInd, input_uv_sub2).rgb;
	// Bent normals are supposed to encode the obscurance in their length, so
	// let's enforce that condition by multiplying by the AO component: (I think it's already weighed; but this kind of enhances the effect)
	//float3 bentN         = /* ssdo.y * */ texBent.Sample(samplerBent, input_uv_sub).xyz; // TBV
	float3 ssaoMask      = texSSAOMask.Sample(samplerSSAOMask, input.uv).xyz;
	float3 ssMask        = texSSMask.Sample(samplerSSMask, input.uv).xyz;
	//float3 emissionMask  = texEmissionMask.Sample(samplerEmissionMask, input_uv_sub).xyz;
	float  mask          = ssaoMask.x;
	float  gloss_mask    = ssaoMask.y;
	float  spec_int_mask = ssaoMask.z;
	float  diff_int      = 1.0;
	float  metallic      = mask / METAL_MAT;
	float  nm_int_mask   = ssMask.x;
	float  spec_val_mask = ssMask.y;
	//bool   shadeless     = mask > GLASS_LO; // SHADELESS_LO;
	// An area is "shadeless" if it's GLASS_MAT or above
	float  shadeless     = saturate((mask - GLASS_LO) / (GLASS_MAT - GLASS_LO)); // Avoid harsh transitions
	//float  diffuse_difference = 1.0;
	// ssMask.z is unused ATM

	// This shader is shared between the SSDO pass and the Deferred pass.
	// For the deferred pass, we don't have an SSDO component, so:
	// Set the ssdo component to 1 if ssdo is disabled
	ssdo = lerp(1.0, ssdo, ssdo_enabled);
	ssdoInd = lerp(0.0, ssdoInd, ssdo_enabled);

	// Toggle the SSDO component for debugging purposes:
	ssdo = lerp(ssdo, 1.0, sso_disable);
	ssdoInd = lerp(ssdoInd, 0.0, sso_disable);

	// Recompute the contact shadow here...
	
	// We need to invert the Z-axis for illumination because the normals are Z+ when viewing the camera
	// so that implies that Z increases towards the viewer and decreases away from the camera.
	// We could also avoid inverting Z in PixelShaderTexture... but then we also need to invert the fake
	// normals.
	const float3 P = pos3D.xyz;
	pos3D.z = -pos3D.z;
	//if (pos3D.z > INFINITY_Z1 || mask > 0.9) // the test with INFINITY_Z1 always adds an ugly cutout line
	// we should either fade gradually between INFINITY_Z0 and INFINITY_Z1, or avoid this test completely.
	
	// Normals with w == 0 are not available -- they correspond to things that don't have
	// normals, like the skybox
	//if (mask > 0.9 || Normal.w < 0.01) {
	// If I remove the following, then the bloom mask is messed up!
	if (Normal.w < 0.001) { // The skybox gets this alpha value
		output.color = float4(color, 1);
		return output;
	}
	// Avoid harsh transitions
	// The substraction below should be 1.0 - Normal.w; but I see alpha = 0.5 coming from the normals buf
	// because that gets written in PixelShaderTexture.hlsl in the alpha channel... I've got to check why
	// later. On the other hand, DC elements have alpha = 1.0 in their normals, so I've got to clamp too
	// or I'll get negative numbers
	shadeless = saturate(shadeless + saturate(2.0 * (0.5 - Normal.w)));

	// Fade shading with distance: works for Yavin, doesn't work for large space missions with planets on them
	// like "Enemy at the Gates"... so maybe enable distance_fade for planetary missions? Those with skydomes...
	float distance_fade = enable_dist_fade * saturate((P.z - INFINITY_Z0) / INFINITY_FADEOUT_RANGE);
	shadeless = saturate(lerp(shadeless, 1.0, distance_fade));

	color = color * color; // Gamma correction (approx pow 2.2)
	float3 N = normalize(Normal.xyz);
	const float3 smoothN = N;
	//const float3 smoothB = bentN;

	// Compute shadows through shadow mapping
	float total_shadow_factor = 1.0;
	//float idx = 1.0;
	if (sm_enabled)
	{
		float shadow_factor = 1.0;
		//float3 P_bias = P + sm_bias * N;
		for (uint i = 0; i < LightCount; i++) 
		{
			float black_level = get_black_level(i);
			// Skip lights that won't project black-enough shadows:
			if (black_level > 0.95)
				continue;
			//float black_level = sm_black_level;
			// Apply the same transform we applied to the geometry when computing the shadow map:
			//float3 Q = mul(lightWorldMatrix, float4(P, 1.0)).xyz;
			//Q.xyz += POV;
			//float3 Q_bias = mul(lightWorldMatrix[i], float4(P_bias, 1.0)).xyz;
			float3 Q = mul(lightWorldMatrix[i], float4(P, 1.0)).xyz;
			//Q.z = Q_bias.z;

			// shadow_factor: 1 -- No shadow
			// shadow_factor: 0 -- Full shadow
			if (sm_PCSS_enabled == 1) {
				// PCSS
				shadow_factor = PCSS(i, Q);
			}
			else {
				// Regular PCF
				if (sm_debug) {
					// Convert to texture coords: this maps -1..1 to 0..1:
					float2 sm_pos = lerp(0, 1, Q.xy * float2(0.5, -0.5) + 0.5);
					// Sample the shadow map and compare
					float sm_Z = texShadowMap.SampleLevel(sampPos, // samplerShadowMap, 
						float3(sm_pos, i), 0).x;
					// Early exit: red color for points "at infinity"
					if (sm_Z > 0.98) {
						output.color = float4(1, 0, 0, 1);
						return output;
					}
					// Now convert the depth-stencil coord (0..1) to metric Z:
					//sm_Z = (sm_Z - 0.5) * OBJrange;
					sm_Z = DepthToMetricZ(i, sm_Z);
					// sm_Z is now in metric space, we can compare it with P.z
					//shadow_factor = sm_Z > Q.z + sm_bias ? 1.0 : 0.0;
					shadow_factor = sm_Z > Q.z ? 1.0 : 0.0;
				}
				else {
					// PCF
					if (sm_hardware_pcf)
						shadow_factor = ShadowMapPCF_HW(i, float3(Q.xy, MetricZToDepth(i, Q.z + sm_bias)), sm_pcss_samples, sm_pcss_radius);
					else
						shadow_factor = ShadowMapPCF(i, float3(Q.xy, MetricZToDepth(i, Q.z + sm_bias)), sm_resolution, sm_pcss_samples, sm_pcss_radius);

					//shadow_factor = texShadowMap.SampleCmpLevelZero(cmpSampler, float3(Q.xy, i), MetricZToDepth(i, Q.z + sm_bias));
					//shadow_factor = texShadowMap.SampleCmpLevelZero(cmpSampler, Q.xy, MetricZToDepth(1, Q.z + sm_bias));
					/*
					float2 sm_pos = lerp(0, 1, Q.xy * float2(0.5, -0.5) + 0.5);
					shadow_factor = texShadowMap.SampleCmpLevelZero(cmpSampler, float3(sm_pos, i), MetricZToDepth(i, Q.z + sm_bias));
					shadow_factor = saturate(shadow_factor);
					*/
					//output.color = float4(shadow_factor, shadow_factor, shadow_factor, 1.0);
					//return output;
				}
			}
			// Limit how black the shadows can be to a minimum of black_level
			shadow_factor = max(shadow_factor, black_level);

			// DEBUG
			if (sm_debug == 1 && shadow_factor < 0.5)
			{
				output.color = float4(0.2, 0.2, 0.5, 1.0);
				return output;
			}
			// DEBUG

			// Accumulate this light's shadow factor with the total shadow factor
			total_shadow_factor *= shadow_factor;
		}
	}
	// Limit the blackness of the shadows to the ambient factor
	//total_shadow_factor = max(total_shadow_factor, ambient);

	// Compute ray-traced shadows
	//float3 SSAO_Normal = float3(N.xy, -N.z);
	// SSAO version:
	//float m_offset = moire_offset * (-pos3D.z * moire_scale);
	//float3 shadow_pos3D = P + SSAO_Normal * m_offset;
	//float shadow = 1;
	//if (shadow_enable) shadow = shadow_factor(shadow_pos3D, max_dist * max_dist).x;

	//ssdo = ambient + ssdo; // * shadow; // Add the ambient component 
	//ssdo = lerp(ssdo, 1, mask);
	//ssdoInd = lerp(ssdoInd, 0, mask);

	// Compute normal mapping
	float2 offset = float2(1.0 / screenSizeX, 1.0 / screenSizeY);
	float3 FakeNormal = 0;
	// Glass, Shadeless and Emission should not have normal mapping:
	//nm_int_mask = lerp(nm_int_mask, 0.0, shadeless);
	if (fn_enable && mask < GLASS_LO) {
		FakeNormal = get_normal_from_color(input.uv, offset, nm_int_mask /*, diffuse_difference */);
		// After the normals have blended, we should restore the length of the bent normal:
		// it should be weighed by AO, which is now in ssdo.y
		//bentN = ssdo.y * blend_normals(bentN, FakeNormal);
		N = blend_normals(N, FakeNormal);
	}
	//output.bent = float4(N * 0.5 + 0.5, 1); // DEBUG PURPOSES ONLY
	
	// ************************************************************************************************
	// MATERIAL PROPERTIES
	// Specular color
	float3 spec_col = 1.0;
	float3 HSV = RGBtoHSV(color);
	// Handle both plastic and metallic materials
	if (mask < METAL_HI) {
		// The tint varies from 0 for plastic materials to 1 for fully metallic mats
		float tint = lerp(0.0, 1.0, metallic);
		float value = lerp(spec_val_mask, 0.0, metallic);
		diff_int = lerp(1.0, 0.0, metallic); // Purely metallic surfaces have no diffuse component (?)
		HSV.y *= tint * saturation_boost;
		HSV.z = HSV.z * lightness_boost + value;
		spec_col = HSVtoRGB(HSV);
	}

	// Glossy exponent
	// We can't have exponent == 0 or we'll see a lot of shading artifacts:
	float exponent = max(global_glossiness * gloss_mask, 0.05);
	float spec_bloom_int = global_spec_bloom_intensity;
	if (GLASS_LO <= mask && mask < GLASS_HI) {
		exponent *= 2.0;
		spec_bloom_int *= 3.0; // Make the glass bloom more
	}
	//float spec_bloom = ssdo.y * spec_int * spec_bloom_int * pow(spec, exponent * bloom_glossiness_mult);
	//spec = ssdo.y * LightInt * spec_int * pow(spec, exponent);
	// ************************************************************************************************

	float3 tmp_color = 0.0;
	float4 tmp_bloom = 0.0;
	float contactShadow = 1.0, diffuse = 0.0, bentDiff = 0.0, smoothDiff = 0.0, spec = 0.0;
	float debug_spec = 0.0;
	uint i;

	// Compute the shading contribution from the main lights
	[loop]
	for (i = 0; i < LightCount; i++)
	{
		float3 L = LightVector[i].xyz; // Lights come with Z inverted from ddraw, so they expect negative Z values in front of the camera
		float LightIntensity = dot(LightColor[i].rgb, 0.333);

		// diffuse component
		//bentDiff   = max(dot(smoothB, L), 0.0);
		smoothDiff = max(dot(smoothN, L), 0.0);
		// I know that bentN is already multiplied by ssdo.x above; but I'm
		// multiplying it again here to make the contact shadows more obvious
		//contactShadow  = 1.0 - clamp(smoothDiff - ssdo.x * ssdo.x * bentDiff, 0.0, 1.0); // This works almost perfect
		contactShadow = 1.0 - saturate(smoothDiff - ssdo.x * ssdo.x); // Use SSDO instead of bentDiff --> also good
		// In low-lighting conditions, maybe we can lerp contactShadow with the AO mask?
		// This helps a little bit; but mutes the spec reflections, need to think more about this
		//contactShadow = lerp(ssdo.y, contactShadow * contactShadow, smoothDiff);
		contactShadow *= contactShadow;

		/*
		if (ssao_debug == 14) {
			float temp = dot(smoothB, L);
			contactShadow = 1.0 - clamp(smoothDiff - temp * temp, 0.0, 1.0);
			//contactShadow = 1.0 - clamp(smoothDiff - bentDiff, 0.0, 1.0);
			//contactShadow = clamp(dot(smoothB, L), 0.0, 1.0); // This wipes out spec component when diffuse goes to 0 -- when the light is right ahead, no spec
			contactShadow *= contactShadow;
		}
		if (ssao_debug == 15) {
			contactShadow = 1.0 - clamp(smoothDiff - bentDiff, 0.0, 1.0);
			contactShadow *= contactShadow;
		}
		if (ssao_debug == 16)
			contactShadow = 1.0 - clamp(smoothDiff - ssdo.x * ssdo.x * bentDiff, 0.0, 1.0);
		if (ssao_debug == 17) {
			contactShadow = 1.0 - clamp(smoothDiff - ssdo.x * ssdo.x * bentDiff, 0.0, 1.0);
			contactShadow *= contactShadow;
		}
		*/

		/*
		if (ssao_debug == 11)
			diffuse = max(dot(bentN, L), 0.0);
		else 
			diffuse = max(dot(N, L), 0.0);
		*/
		diffuse = max(dot(N, L), 0.0);
		//diffuse = /* min(shadow, ssdo.x) */ ssdo.x * diff_int * diffuse + ambient;
		diffuse = min(total_shadow_factor, ssdo.x) * diff_int * diffuse + ambient;

		// Default case
		//diffuse = ssdo.x * diff_int * diffuse + ambient; // ORIGINAL
		//diffuse = diff_int * diffuse + ambient;

		//diffuse = lerp(diffuse, 1, mask); // This applies the shadeless material; but it's now defined differently
		/*
		if (shadeless) {
			diffuse = 1.0;
			contactShadow = 1.0;
			ssdoInd = 0.0;
		}
		*/

		// Avoid harsh transitions:
		// shadeless surfaces should still receive some amount of shadows
		diffuse = lerp(diffuse, min(total_shadow_factor + 0.5, 1.0), shadeless);
		contactShadow = lerp(contactShadow, 1.0, shadeless);
		ssdoInd = lerp(ssdoInd, 0.0, shadeless);

		// specular component
		float3 eye_vec = normalize(-pos3D); // normalize(eye - pos3D);
		// reflect expects an incident vector: a vector that goes from the light source to the current point.
		// L goes from the current point to the light vector, so we have to use -L:
		float3 refl_vec = normalize(reflect(-L, N));
		spec = max(dot(eye_vec, refl_vec), 0.0);

		//const float3 H = normalize(L + eye_vec);
		//spec = max(dot(N, H), 0.0);
		
		// TODO REMOVE CONTACT SHADOWS FROM SSDO DIRECT (Probably done already)
		float spec_bloom = contactShadow * spec_int_mask * spec_bloom_int * pow(spec, exponent * global_bloom_glossiness_mult);
		debug_spec = LightIntensity * spec_int_mask * pow(spec, exponent);
		//spec = /* min(contactShadow, shadow) */ contactShadow * debug_spec;
		spec = min(contactShadow, total_shadow_factor) * debug_spec;

		// Avoid harsh transitions (the lines below will also kill glass spec)
		//spec_col = lerp(spec_col, 0.0, shadeless);
		//spec_bloom = lerp(spec_bloom, 0.0, shadeless);
		
		// The following lines MAY be an alternative to remove spec on shadeless surfaces; keeping glass
		// intact
		//spec_col = mask > SHADELESS_LO ? 0.0 : spec_col;
		//spec_bloom = mask > SHADELESS_LO ? 0.0 : spec_bloom;

		//color = color * ssdo + ssdoInd + ssdo * spec_col * spec;
		tmp_color += LightColor[i].rgb * saturate(
			color * diffuse +
			global_spec_intensity * spec_col * spec +
			/* diffuse_difference * */ /* color * */ ssdoInd); // diffuse_diff makes it look cartoonish, and mult by color destroys the effect
			//emissionMask);
		//tmp_bloom += /* min(shadow, contactShadow) */ contactShadow * float4(LightIntensity * spec_col * spec_bloom, spec_bloom);
		tmp_bloom += min(total_shadow_factor, contactShadow) * float4(LightIntensity * spec_col * spec_bloom, spec_bloom);
		
	}
	output.bloom = tmp_bloom;

	// Add the laser lights
#define L_FADEOUT_A_0 30.0
#define L_FADEOUT_A_1 50.0
#define L_FADEOUT_B_0 50.0
#define L_FADEOUT_B_1 1000.0
	float3 laser_light_sum = 0.0;
	//float laser_light_alpha = 0.0;
	[loop]
	for (i = 0; i < num_lasers; i++)
	{
		// P is the original point, pos3D has z inverted:
		// pos3D = (P.xy, -P.z)
		// LightPoint already comes with z inverted (-z) from ddraw, so we can subtract LightPoint - pos3D
		// because both points have -z:
		float3 L = LightPoint[i].xyz - pos3D;
		const float Z = -LightPoint[i].z; // Z is positive depth
		
		const float distance_sqr = dot(L, L);
		L *= rsqrt(distance_sqr); // Normalize L
		// calculate the attenuation
		const float depth_attenuation_A = smoothstep(L_FADEOUT_A_1, L_FADEOUT_A_0, Z); // Fade the cockpit flash quickly
		const float depth_attenuation_B = 0.1 * smoothstep(L_FADEOUT_B_1, L_FADEOUT_B_0, Z); // Fade the distant flash slowly
		const float depth_attenuation = max(depth_attenuation_A, depth_attenuation_B);
		//const float sqr_attenuation_faded = lerp(sqr_attenuation, 0.0, 1.0 - depth_attenuation);
		//const float sqr_attenuation_faded = lerp(sqr_attenuation, 1.0, saturate((Z - L_SQR_FADE_0) / L_SQR_FADE_1));
		const float attenuation = 1.0 / (1.0 + sqr_attenuation * distance_sqr);
		// compute the diffuse contribution
		const float diff_val = max(dot(N, L), 0.0); // Compute the diffuse component
		//laser_light_alpha += diff_val;
		// add everything up
		laser_light_sum += depth_attenuation * attenuation * diff_val * LightPointColor[i].rgb;
	}
	tmp_color += laser_light_intensity * laser_light_sum;
	// Blend the existing tmp_bloom with the new one:
	//laser_light_alpha = saturate(laser_light_alpha);
	//tmp_bloom += float4(laser_light_sum, laser_light_alpha);
	////tmp_bloom.a = max(tmp_bloom.a, laser_light_alpha); // Modifying the alpha fades the bloom too -- not a good idea

	output.color = float4(sqrt(tmp_color), 1); // Invert gamma correction (approx pow 1/2.2)
	

#ifdef DISABLED
	if (ssao_debug == 8)
		output.color.xyz = bentN.xyz * 0.5 + 0.5;
	if (ssao_debug == 9 || ssao_debug >= 14)
		output.color.xyz = contactShadow;
	if (ssao_debug == 10)
		output.color.xyz = bentDiff;
	if (ssao_debug == 12)
		output.color.xyz = color * (diff_int * bentDiff + ambient);
	if (ssao_debug == 13)
		output.color.xyz = N.xyz * 0.5 + 0.5;
	if (ssao_debug == 18)
		output.color.xyz = smoothDiff;
	if (ssao_debug == 19)
		output.color.xyz = spec;
	if (ssao_debug == 20)
		output.color.xyz = debug_spec;
	if (ssao_debug == 21)
		output.color.xyz = diffuse_difference;
	//if (ssao_debug == 22)
	//	output.color.xyz = shadow;
	if (ssao_debug == 23)
		output.color.xyz = ssdoInd; // Should display the mask instead of the ssdoInd color
	if (ssao_debug == 24)
		output.color.xyz = ssdoInd;
	if (ssao_debug == 25)
		output.color.xyz = ssdoInd; // Should display a debug value coming from SSDOInd
	if (ssao_debug == 26)
		output.color.xyz = 0.5 * ssdoInd;
#endif

	return output;
	//return float4(pow(abs(color), 1/gamma) * ssdo + ssdoInd, 1);


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
