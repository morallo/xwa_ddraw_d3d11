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
#include "..\RT\RTCommon.h"

// The color buffer
Texture2D texColor : register(t0);
SamplerState sampColor : register(s0);

// The SSAO buffer
Texture2D texSSAO : register(t1);
SamplerState samplerSSAO : register(s1);

// s2/t2: 
// indirect SSDO buffer

// The SSAO mask
Texture2D texSSAOMask : register(t3);
SamplerState samplerSSAOMask : register(s3);


// The position/depth buffer
Texture2D texPos : register(t4);
SamplerState sampPos : register(s4);

// The (Smooth) Normals buffer
Texture2D texNormal : register(t5);
SamplerState samplerNormal : register(s5);

// s6/t6: 
// Bent Normals buffer

// The Shading System Mask buffer
Texture2D texSSMask : register(t7);
SamplerState samplerSSMask : register(s7);

// The Shadow Map buffer
Texture2DArray<float> texShadowMap : register(t8);
SamplerComparisonState cmpSampler : register(s8);

// The RT Shadow Mask
Texture2D rtShadowMask : register(t17);

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

cbuffer ConstantBuffer : register(b4)
{
	matrix projEyeMatrix;
	matrix viewMatrix;
	matrix fullViewMatrix;
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
};

// From https://64.github.io/tonemapping/
inline float3 reinhard_extended(float3 v, float max_white)
{
	float3 numerator = v * (1.0f + (v / (max_white * max_white)));
	return numerator / (1.0f + v);
}

inline float3 getPosition(in float2 uv, in float level) {
	// The use of SampleLevel fixes the following error:
	// warning X3595: gradient instruction used in a loop with varying iteration
	// This happens because the texture is sampled within an if statement (if FGFlag then...)
	return texPos.SampleLevel(sampPos, uv, level).xyz;
}

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

PixelShaderOutput main(PixelShaderInput input)
{
	float2 input_uv_sub	  = input.uv * amplifyFactor;
	float3 color          = texColor.Sample(sampColor, input.uv).xyz;
	float4 Normal         = texNormal.Sample(samplerNormal, input.uv);
	float3 ssao           = texSSAO.Sample(samplerSSAO, input_uv_sub).rgb;
	float3 ssaoMask       = texSSAOMask.Sample(samplerSSAOMask, input.uv).xyz;
	float3 ssMask         = texSSMask.Sample(samplerSSMask, input.uv).xyz;
	float  mask           = ssaoMask.x;
	float  gloss_mask     = ssaoMask.y;
	float  spec_int_mask  = ssaoMask.z;
	float  diff_int       = 1.0;
	float  metallic       = mask / METAL_MAT;
	float  glass          = ssMask.x;
	float  spec_val       = ssMask.y;
	float  shadeless      = ssMask.z;
	float3 pos3D;

	PixelShaderOutput output;
	output.color = 0;
	output.bloom = 0;

	if (shadeless > 0.9) {
		output.color = float4(color, 1);
		return output;
	}

	// DEBUG
	//output.color = float4(ssao, 1);
	//return output;
	// DEBUG

	// Normals with w == 0 are not available -- they correspond to things that don't have
	// normals, like the skybox
	//if (mask > 0.9 || Normal.w < 0.01) {
	// The skybox gets alpha = 0, but when MSAA is on, alpha goes from 0 to 0.5
	if (Normal.w < 0.475) {
		output.color = float4(color, 1);
		return output;
	}
	// Avoid harsh transitions
	// The substraction below should be 1.0 - Normal.w; but I see alpha = 0.5 coming from the normals buf
	// because that gets written in PixelShaderTexture.hlsl in the alpha channel... I've got to check why
	// later. On the other hand, DC elements have alpha = 1.0 in their normals, so I've got to clamp too
	// or I'll get negative numbers
	shadeless = saturate(shadeless + saturate(2.0 * (0.5 - Normal.w)));

	color = color * color; // Gamma correction (approx pow 2.2)
	ssao = saturate(pow(abs(ssao), power)); // Increase ssao contrast
	float3 N = normalize(Normal.xyz);

	// For shadeless areas, make ssao 1
	//ssao = shadeless ? 1.0 : ssao;
	ssao = lerp(ssao, 1.0, shadeless);
	if (ssao_debug) {
		ssao = sqrt(ssao); // Gamma correction
		output.color = float4(ssao, 1);
		return output;
	}
	// Toggle the SSAO component for debugging purposes:
	ssao = lerp(ssao, 1.0, sso_disable);

	float3 P = getPosition(input.uv, 0);
	// We need to invert the Z-axis for illumination because the normals are Z+ when viewing the camera
	// so that implies that Z increases towards the viewer and decreases away from the camera.
	// We could also avoid inverting Z in PixelShaderTexture... but then we also need to invert the fake
	// normals.
	pos3D = float3(P.xy, -P.z);

	// Fade shading with distance: works for Yavin, doesn't work for large space missions with planets on them
	// like "Enemy at the Gates"... so maybe enable distance_fade for planetary missions? Those with skydomes...
	float distance_fade = enable_dist_fade * saturate((pos3D.z - INFINITY_Z0) / INFINITY_FADEOUT_RANGE);
	shadeless = saturate(lerp(shadeless, 1.0, distance_fade));

	// Raytraced shadows. The output of this section is written to rt_shadow_factor
	float rt_shadow_factor = 1.0f;
	if (bRTEnabled)
	{
		if (RTEnableSoftShadows)
		{
			int i, j;
			float2 uv;
			float rtVal = 0;
			const int range = 2;
			const float rtCenter = rtShadowMask.Sample(sampColor, input.uv).x;
			//const float wsize = (2 * range + 1) * (2 * range + 1);
			const float2 uv_delta = float2(RTShadowMaskPixelSizeX * RTShadowMaskSizeFactor,
				RTShadowMaskPixelSizeY * RTShadowMaskSizeFactor);
			const float2 uv_delta_d = /* 0.75 * */ uv_delta; // Dilation filter delta
			const float2 uv_delta_r = range * uv_delta; // Soft shadow delta

			// Apply a quick-and-dirty dilation filter to prevent haloes
			const float2 uv0_d = input.uv - uv_delta_d;
			float rtMin = rtCenter;
			//if (RTApplyDilateFilter)
			{
				uv = uv0_d;
				[unroll]
				for (i = -1; i <= 1; i++)
				{
					uv.x = uv0_d.x;
					[unroll]
					for (j = -1; j <= 1; j++)
					{
						rtMin = min(rtMin, rtShadowMask.Sample(sampColor, uv).x);
						uv.x += uv_delta_d.x;
					}
					uv.y += uv_delta_d.y;
				}
			}

			float Gsum = 0;
			const float2 uv0_r = input.uv - range * uv_delta_r;
			uv = uv0_r;
			//float gaussFactor = clamp(1.5 - rtCenter.y * 0.1, 0.01, 1.5);
			[unroll]
			for (i = -range; i <= range; i++)
			{
				uv.x = uv0_r.x;
				[unroll]
				for (j = -range; j <= range; j++)
				{
					const float z = texPos.Sample(sampPos, uv).z;
					const float delta_z = abs(P.z - z);
					const float delta_ij = -RTGaussFactor * (i * i + j * j);
					//const float G = RTUseGaussFilter ? exp(delta_ij) : 1.0;
					const float G = exp(delta_ij);
					// Objects far away should have bigger thresholds too
					if (delta_z < RTSoftShadowThresholdMult * P.z)
						rtVal += G * rtShadowMask.Sample(sampColor, uv).x;
					else
						rtVal += G * rtMin;
					Gsum += G;
					uv.x += uv_delta_r.x;
				}
				uv.y += uv_delta_r.y;
			}
			rt_shadow_factor = rtVal / Gsum;
		}
		else
		{
			// Do ray-tracing right here, no RTShadowMask
			for (uint i = 0; i < LightCount; i++)
			{
				float black_level, dummyMinZ, dummyMaxZ;
				get_black_level_and_minmaxZ(i, black_level, dummyMinZ, dummyMaxZ);
				// Skip lights that won't project black-enough shadows
				if (black_level > 0.95)
					continue;

				const float3 L = LightVector[i].xyz;
				const float dotLFlatN = dot(L, N); // The "flat" normal is needed here (without Normal Mapping)
				// "hover" prevents noise by displacing the origin of the ray away from the surface
				// The displacement is directly proportional to the depth of the surface
				// The position buffer's Z increases with depth
				// The normal buffer's Z+ points towards the camera
				// We have to invert N.z:
				const float3 hover = 0.01 * P.z * float3(N.x, N.y, -N.z);
				// Don't do raytracing on surfaces that face away from the light source
				if (dotLFlatN > 0.01)
				{
					Ray ray;
					ray.origin = P + hover; // Metric, Y+ is up, Z+ is forward.
					ray.dir = float3(L.x, -L.y, -L.z);
					ray.max_dist = RT_MAX_DIST;

					Intersection inters = TLASTraceRaySimpleHit(ray);
					if (inters.TriID > -1)
						rt_shadow_factor *= black_level;
				}
				else
				{
					rt_shadow_factor *= black_level;
				}
			}
		}
	}

	// Compute shadows through shadow mapping
	float total_shadow_factor = rt_shadow_factor;
	if (sm_enabled && bRTAllowShadowMapping)
	{
		//float3 P_bias = P + sm_bias * N;
		[loop]
		for (uint i = 0; i < LightCount; i++)
		{
			float shadow_factor = 1.0;
			float black_level, minZ, maxZ;
			get_black_level_and_minmaxZ(i, black_level, minZ, maxZ);
			// Skip lights that won't project black-enough shadows, and skip
			// lights that are out-of-range.
			if (black_level > 0.95 || P.z < minZ || P.z > maxZ)
				continue;
			// Apply the same transform we applied to in ShadowMapVS.hlsl
			float3 Q = mul(lightWorldMatrix[i], float4(P, 1.0)).xyz;
			// Distant objects require more bias, here we're using maxZ as a proxy to tell us
			// how distant is the shadow map and we use that to compensate the bias
			float bias = sm_bias - 1.0 * step(50.0f, maxZ);
			// shadow_factor: 1 -- No shadow
			// shadow_factor: 0 -- Full shadow
			shadow_factor = ShadowMapPCF(i, float3(Q.xy, MetricZToDepth(i, Q.z + bias)), sm_resolution, sm_pcss_samples, sm_pcss_radius);
			// Limit how black the shadows can be to a minimum of black_level
			shadow_factor = max(shadow_factor, black_level);

			// Accumulate this light's shadow factor with the total shadow factor
			total_shadow_factor *= shadow_factor;
		}
	}

	// ************************************************************************************************
	// MATERIAL PROPERTIES
	// Specular color
	float3 spec_col = 1.0;
	float3 HSV = RGBtoHSV(color);
	// Handle both plastic and metallic materials
	if (mask < METAL_HI) {
		// The tint varies from 0 for plastic materials to 1 for fully metallic mats
		float tint = lerp(0.0, 1.0, metallic);
		float value = lerp(spec_val, 0.0, metallic);
		diff_int = lerp(1.0, 0.0, metallic); // Purely metallic surfaces have no diffuse component (?)
		HSV.y *= tint * saturation_boost;
		HSV.z = HSV.z * lightness_boost + value;
		spec_col = HSVtoRGB(HSV);
	}

	// We can't have exponent == 0 or we'll see a lot of shading artifacts:
	float exponent = max(global_glossiness * gloss_mask, 0.05);
	float spec_bloom_int = global_spec_bloom_intensity;
	//if (GLASS_LO <= mask && mask < GLASS_HI)
	if (glass > 0.01)
	{
		exponent *= 2.0;
		spec_bloom_int *= 3.0; // Make the glass bloom more
	}
	// ************************************************************************************************

	float3 tmp_color = 0.0;
	float4 tmp_bloom = 0.0;
	// Compute the shading contribution from the main lights
	[unroll]
	for (uint i = 0; i < LightCount; i++) {
		float3 L = LightVector[i].xyz; // Lights come with Z inverted from ddraw, so they expect negative Z values in front of the camera
		float LightIntensity = dot(LightColor[i].rgb, 0.333);
		// diffuse component
		float diffuse = max(dot(N, L), 0.0);
		diffuse = total_shadow_factor * ssao.x * diff_int * diffuse + ambient;
		//diffuse = (diff_max * diff_int * diffuse) + (ssao.x * amb_max * ambient);
		//diffuse = diff_int * diffuse + ambient;

		//diffuse = shadeless ? 1.0 : diffuse;
		// shadeless surfaces should still receive some amount of shadows
		diffuse = lerp(diffuse, min(total_shadow_factor + 0.5, 1.0), shadeless);

		// specular component
		//float3 eye = 0.0;
		//float3 spec_col = lerp(min(6.0 * color, 1.0), 1.0, mask); // Force spec_col to be white on masked (DC) areas
		//float3 spec_col = 0.35;
		float3 eye_vec = normalize(-pos3D); // normalize(eye - pos3D);
		// reflect expects an incident vector: a vector that goes from the light source to the current point.
		// L goes from the current point to the light vector, so we have to use -L:
		float3 refl_vec = normalize(reflect(-L, N));
		float  spec = max(dot(eye_vec, refl_vec), 0.0);

		//float3 viewDir    = normalize(-pos3D);
		//float3 halfwayDir = normalize(L + viewDir);
		//float spec = max(dot(N, halfwayDir), 0.0);

		float spec_bloom = spec_int_mask * spec_bloom_int * pow(spec, exponent * global_bloom_glossiness_mult);
		spec = total_shadow_factor * LightIntensity * spec_int_mask * pow(spec, exponent);

		//color = color * ssdo + ssdoInd + ssdo * spec_col * spec;
		tmp_color += LightColor[i].rgb * (color * diffuse + global_spec_intensity * spec_col * spec);
		tmp_bloom += total_shadow_factor * float4(LightIntensity * spec_col * spec_bloom, spec_bloom);
	}
	output.bloom = tmp_bloom;

	// Add the laser/dynamic lights
#define L_FADEOUT_A_0 30.0
#define L_FADEOUT_A_1 50.0
#define L_FADEOUT_B_0 50.0
#define L_FADEOUT_B_1 1000.0
	float3 laser_light_sum = 0.0;
	//float laser_light_alpha = 0.0;
	[loop]
	for (i = 0; i < num_lasers; i++)
	{
		// P is the original point
		// pos3D = (P.xy, -P.z)
		// LightPoint already comes with z inverted (-z) from ddraw
		float3 L = LightPoint[i].xyz - pos3D;
		const float Z = -LightPoint[i].z;
		const float falloff = LightPoint[i].w;
		const float3 LDir = LightPointDirection[i].xyz;
		const float angle_falloff_cos = LightPointDirection[i].w;

		float attenuation, depth_attenuation, angle_attenuation = 1.0f;
		const float distance_sqr = dot(L, L);
		L *= rsqrt(distance_sqr); // Normalize L
		// calculate the attenuation for laser lights
		if (falloff == 0.0f) {
			const float depth_attenuation_A = 0.15 * smoothstep(L_FADEOUT_A_1, L_FADEOUT_A_0, Z); // Fade the cockpit flash quickly
			const float depth_attenuation_B = 0.1 * smoothstep(L_FADEOUT_B_1, L_FADEOUT_B_0, Z); // Fade the distant flash slowly
			depth_attenuation = max(depth_attenuation_A, depth_attenuation_B);
			//const float sqr_attenuation_faded = lerp(sqr_attenuation, 0.0, 1.0 - depth_attenuation);
			//const float sqr_attenuation_faded = lerp(sqr_attenuation, 1.0, saturate((Z - L_SQR_FADE_0) / L_SQR_FADE_1));
			attenuation = 1.0 / (1.0 + sqr_attenuation * distance_sqr);
		}
		// calculate the attenuation for other lights (explosions, etc)
		else {
			depth_attenuation = 1.0;
			attenuation = falloff / distance_sqr;
		}
		// calculate the attenation for directional lights
		if (angle_falloff_cos != 0.0f) {
			// compute the angle between the light's direction and the
			// vector from the current point to the light's center
			const float angle = max(dot(L, LDir), 0.0);
			angle_attenuation = smoothstep(angle_falloff_cos, 1.0, angle);
		}
		// compute the diffuse contribution
		const float diff_val = max(dot(N, L), 0.0); // Compute the diffuse component
		//laser_light_alpha += diff_val;
		// add everything up
		laser_light_sum += depth_attenuation * attenuation * angle_attenuation * diff_val * LightPointColor[i].rgb;
	}
	//laser_light_sum = laser_light_sum / (laser_light_intensity + laser_light_sum);
	tmp_color += laser_light_intensity * laser_light_sum;

	// Reinhard tone mapping:
	if (HDREnabled) {
		tmp_color = tmp_color / (HDR_white_point + tmp_color);
		//tmp_color = reinhard_extended(tmp_color, HDR_white_point);
	}
	output.color = float4(sqrt(tmp_color), 1); // Invert gamma correction (approx pow 1/2.2)
	return output;
}
