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
//#include "..\PBRShading.h"
#include "..\RT\RTCommon.h"

#undef PBR_DYN_LIGHTS
#undef RT_SIDE_LIGHTS

//#define PBR_DYN_LIGHTS
SamplerState sampler0 : register(s0);

#ifdef INSTANCED_RENDERING
// The color buffer
Texture2DArray texColor : register(t0);

// The SSDO Direct buffer
Texture2DArray texSSDO : register(t1);

// The SSDO Indirect buffer
//Texture2DArray texSSDOInd : register(t2);
// The Background buffer
Texture2DArray texBackground : register(t2);

// The SSAO mask
Texture2DArray texSSAOMask : register(t3);

// The position/depth buffer
Texture2DArray texPos : register(t4);

// The (Smooth) Normals buffer
Texture2DArray texNormal : register(t5);

// The Bent Normals buffer
//Texture2DArray texBent : register(t6);

// The Shading System Mask buffer
Texture2DArray texSSMask : register(t6);

// The RT Shadow Mask
Texture2DArray rtShadowMask : register(t17);

// Transparent layer 1
Texture2DArray transp1 : register(t18);
// Transparent layer 2
Texture2DArray transp2 : register(t19);
#else
// The color buffer
Texture2D texColor : register(t0);

// The SSDO Direct buffer
Texture2D texSSDO : register(t1);

// The SSDO Indirect buffer
//Texture2D texSSDOInd : register(t2);
// The Background buffer
Texture2D texBackground : register(t2);

// The SSAO mask
Texture2D texSSAOMask : register(t3);

// The position/depth buffer
Texture2D texPos : register(t4);

// The (Smooth) Normals buffer
Texture2D texNormal : register(t5);

// The Shading System Mask buffer
Texture2D texSSMask : register(t6);

// The RT Shadow Mask
Texture2D rtShadowMask : register(t17);

// Transparent layer 1
Texture2D transp1 : register(t18);
// Transparent layer 2
Texture2D transp2 : register(t19);

// Reticle (only available in non VR mode)
Texture2D reticleTex : register(t20);
#endif

// The Shadow Map buffer
Texture2DArray<float> texShadowMap : register(t7);
SamplerComparisonState cmpSampler : register(s7);

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
	float4 pos    : SV_POSITION;
	float2 uv     : TEXCOORD;
#ifdef INSTANCED_RENDERING
	uint   viewId : SV_RenderTargetArrayIndex;
#endif
};

struct PixelShaderOutput
{
	float4 color : SV_TARGET0;
	float4 bloom : SV_TARGET1;
	//float4 bent  : SV_TARGET2;
};

// From: https://www.shadertoy.com/view/MdfXWr
float3 ff_filmic_gamma3(float3 linearc) {
	float3 x = max(0.0, linearc - 0.004);
	return (x*(x*6.2 + 0.5)) / (x*(x*6.2 + 1.7) + 0.06);
}

// From: https://www.shadertoy.com/view/wl2SDt
float3 ACESFilm(float3 x)
{
	float a = 2.51;
	float b = 0.03;
	float c = 2.43;
	float d = 0.59;
	float e = 0.14;
	return (x*(a*x + b)) / (x*(c*x + d) + e);
}

// From: https://64.github.io/tonemapping/
float3 ACES_approx(float3 v)
{
	v *= 0.6f;
	float a = 2.51f;
	float b = 0.03f;
	float c = 2.43f;
	float d = 0.59f;
	float e = 0.14f;
	return clamp((v*(a*v + b)) / (v*(c*v + d) + e), 0.0f, 1.0f);
}

// From: https://64.github.io/tonemapping/
float3 uncharted2_tonemap_partial(float3 x)
{
	float A = 0.15f;
	float B = 0.50f;
	float C = 0.10f;
	float D = 0.20f;
	float E = 0.02f;
	float F = 0.30f;
	return ((x*(A*x + C * B) + D * E) / (x*(A*x + B) + D * F)) - E / F;
}

float3 uncharted2_filmic(float3 v)
{
	float exposure_bias = 2.0f;
	float3 curr = uncharted2_tonemap_partial(v * exposure_bias);

	float3 W = 11.2f;
	float3 white_scale = 1.0 / uncharted2_tonemap_partial(W);
	return curr * white_scale;
}

// From https://64.github.io/tonemapping/
inline float3 reinhard_extended(float3 v, float max_white)
{
	float3 numerator = v * (1.0f + (v / (max_white * max_white)));
	return numerator / (1.0f + v);
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
			float4 tmp = texShadowMap.Gather(sampler0, //samplerShadowMap,
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

// See:
// https://wickedengine.net/2017/10/22/which-blend-state-for-me/
inline float4 PreMulBlend(in float4 src, in float4 dst)
{
	// Equivalent DX11 state:
	// desc.SrcBlend       = D3D11_BLEND_ONE;
	// desc.DestBlend      = D3D11_BLEND_INV_SRC_ALPHA;
	// desc.BlendOp        = D3D11_BLEND_OP_ADD;
	// Final alpha value settings:
	// desc.SrcBlendAlpha  = D3D11_BLEND_ONE;
	// desc.DestBlendAlpha = D3D11_BLEND_ONE;
	// desc.BlendOpAlpha   = D3D11_BLEND_OP_ADD;
	dst.rgb = src.rgb + (1 - src.a) * dst.rgb;
	return dst;
}

float4 BlendTransparentLayers(in float4 color, in float4 transpColor1, in float4 transpColor2)
{
	// Blend the transparent layers now
	//if (!ssao_debug)
	{
		color = PreMulBlend(transpColor1, color);
		color = PreMulBlend(transpColor2, color);
	}
	return color;
}

float4 BlendTransparentLayers(
	in float4 color,
	in float4 transpColor1,
	in float4 transpColor2,
	in float4 transpColor3)
{
	// Blend the transparent layers now
	//if (!ssao_debug)
	{
		color = PreMulBlend(transpColor1, color);
		color = PreMulBlend(transpColor2, color);
		color = PreMulBlend(transpColor3, color);
	}
	return color;
}

PixelShaderOutput main(PixelShaderInput input)
{
	PixelShaderOutput output;
	output.color = 0;
	output.bloom = 0;
	//output.bent  = 0;

	float2 input_uv_sub   = input.uv * amplifyFactor;
	//float2 input_uv_sub2 = input.uv * amplifyFactor2;
	float2 input_uv_sub2  = input.uv * amplifyFactor;
#ifdef INSTANCED_RENDERING
	float4 texelColor   = texColor.Sample(sampler0, float3(input.uv,input.viewId));
	float4 Normal       = texNormal.Sample(sampler0, float3(input.uv, input.viewId));
	float3 pos3D	    = texPos.Sample(sampler0, float3(input.uv, input.viewId)).xyz;
	float3 ssdo         = texSSDO.Sample(sampler0, float3(input_uv_sub, input.viewId)).rgb;
	//float3 ssdoInd    = texSSDOInd.Sample(sampler0, float3(input_uv_sub2, input.viewId)).rgb;
	float3 background   = texBackground.Sample(sampler0, float3(input.uv, input.viewId)).rgb;

	// Bent normals are supposed to encode the obscurance in their length, so
	// let's enforce that condition by multiplying by the AO component: (I think it's already weighed; but this kind of enhances the effect)
	//float3 bentN         = /* ssdo.y * */ texBent.Sample(samplerBent, input_uv_sub).xyz; // TBV
	float3 ssaoMask     = texSSAOMask.Sample(sampler0, float3(input.uv, input.viewId)).xyz;
	float3 ssMask       = texSSMask.Sample(sampler0, float3(input.uv, input.viewId)).xyz;
	float4 transpColor1 = transp1.Sample(sampler0, float3(input.uv, input.viewId));
	float4 transpColor2 = transp2.Sample(sampler0, float3(input.uv, input.viewId));
#else
	float4 texelColor = texColor.Sample(sampler0, input.uv);
	float4 Normal     = texNormal.Sample(sampler0, input.uv);
	float3 pos3D      = texPos.Sample(sampler0, input.uv).xyz;
	float3 ssdo       = texSSDO.Sample(sampler0, input_uv_sub).rgb;
	//float3 ssdoInd  = texSSDOInd.Sample(sampler0, input_uv_sub2).rgb;
	float3 background = texBackground.Sample(sampler0, input.uv).rgb;

	// Bent normals are supposed to encode the obscurance in their length, so
	// let's enforce that condition by multiplying by the AO component: (I think it's already weighed; but this kind of enhances the effect)
	//float3 bentN         = /* ssdo.y * */ texBent.Sample(samplerBent, input_uv_sub).xyz; // TBV
	float3 ssaoMask     = texSSAOMask.Sample(sampler0, input.uv).xyz;
	float3 ssMask       = texSSMask.Sample(sampler0, input.uv).xyz;
	float4 transpColor1 = transp1.Sample(sampler0, input.uv);
	float4 transpColor2 = transp2.Sample(sampler0, input.uv);
	float4 reticleColor = reticleTex.Sample(sampler0, input.uv);
#endif
	float3 color          = texelColor.rgb;
	float  mask           = ssaoMask.x;
	float  gloss_mask     = ssaoMask.y;
	float  spec_int_mask  = ssaoMask.z;
	float  diff_int       = 1.0;
	float  metallic       = mask / METAL_MAT;
	float  glass          = ssMask.x;
	float  spec_val_mask  = ssMask.y;
	float  shadeless      = ssMask.z;

	const float blendAlphaSat = saturate(2.0 * Normal.w);
	const float blendAlpha = blendAlphaSat * blendAlphaSat;

	// This shader is shared between the SSDO pass and the Deferred pass.
	// For the deferred pass, we don't have an SSDO component, so:
	// Set the ssdo component to 1 if ssdo is disabled
	ssdo = lerp(1.0, ssdo, ssdo_enabled);
	//ssdoInd = lerp(0.0, ssdoInd, ssdo_enabled);

	// Toggle the SSDO component for debugging purposes:
	ssdo = lerp(ssdo, 1.0, sso_disable);
	//ssdoInd = lerp(ssdoInd, 0.0, sso_disable);

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
	// The skybox gets alpha = 0, but when MSAA is on, alpha goes from 0 to 0.5
	/*
	if (Normal.w < 0.475) {
		output.color = float4(color, 1);
		return output;
	}
	*/

	if (shadeless >= 1.0)
	{
		output.color = float4(lerp(background, color, blendAlpha), 1);
#ifdef INSTANCED_RENDERING
		output.color = BlendTransparentLayers(output.color, transpColor1, transpColor2);
#else
		output.color = BlendTransparentLayers(output.color, transpColor1, transpColor2, reticleColor);
#endif
		return output;
	}

	// Avoid harsh transitions
	// The substraction below should be 1.0 - Normal.w; but I see alpha = 0.5 coming from the normals buf
	// because that gets written in PixelShaderTexture.hlsl in the alpha channel... I've got to check why
	// later. On the other hand, DC elements have alpha = 1.0 in their normals, so I've got to clamp too
	// or I'll get negative numbers
	//shadeless = saturate(shadeless + saturate(2.0 * (0.5 - Normal.w)));

	// Fade shading with distance: works for Yavin, doesn't work for large space missions with planets on them
	// like "Enemy at the Gates"... so maybe enable distance_fade for planetary missions? Those with skydomes...
	float distance_fade = enable_dist_fade * saturate((P.z - INFINITY_Z0) / INFINITY_FADEOUT_RANGE);
	//shadeless = saturate(lerp(shadeless, 1.0, distance_fade));

	color = color * color; // Gamma correction (approx pow 2.2)

	float3 N = normalize(Normal.xyz);
	const float3 smoothN = N;
	//const float3 smoothB = bentN;

	// Raytraced shadows. The output of this section is written to rt_shadow_factor
	float rt_shadow_factor = 1.0f;
	if (bRTEnabled)
	{
		if (RTEnableSoftShadows)
		{
			// Hard shadows:
			/*{
				float4 rtVal = rtShadowMask.Sample(sampler0, input.uv);
				rt_shadow_factor = rtVal.x;
			}*/
			{
				int i, j;
				float2 uv;
				float rtVal = 0;
				const int range = 2;
#ifdef INSTANCED_RENDERING
                const float rtCenter = rtShadowMask.Sample(sampler0, float3(input.uv, input.viewId)).x;
#else
                const float rtCenter = rtShadowMask.Sample(sampler0, input.uv).x;
#endif
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
#ifdef INSTANCED_RENDERING
                            rtMin = min(rtMin, rtShadowMask.Sample(sampler0, float3(uv, input.viewId)).x);
#else
                            rtMin = min(rtMin, rtShadowMask.Sample(sampler0, uv).x);
#endif
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
#ifdef INSTANCED_RENDERING
						const float z = texPos.Sample(sampler0, float3(input.uv, input.viewId)).z;
#else
                        const float z = texPos.Sample(sampler0, input.uv).z;
#endif
						const float delta_z = abs(P.z - z);
						const float delta_ij = -RTGaussFactor * (i*i + j*j);
						//const float G = RTUseGaussFilter ? exp(delta_ij) : 1.0;
						const float G = exp(delta_ij);
						// Objects far away should have bigger thresholds too
						if (delta_z < RTSoftShadowThresholdMult * P.z)
#ifdef INSTANCED_RENDERING
                            rtVal += G * rtShadowMask.Sample(sampler0, float3(uv, input.viewId)).x;
#else
                            rtVal += G * rtShadowMask.Sample(sampler0, uv).x;
#endif
						else
							rtVal += G * rtMin;
						Gsum += G;
						uv.x += uv_delta_r.x;
					}
					uv.y += uv_delta_r.y;
				}
				rt_shadow_factor = rtVal / Gsum;
			}
		}
		else
		{
			// Do ray-tracing right here, no RTShadowMask

			// #define RT_SIDE_LIGHTS to disable light tagging and use the first light only.
			// This helps get interesting shadows in skirmish missions.
#ifdef RT_SIDE_LIGHTS
			uint i = 0;
#else
			for (uint i = 0; i < LightCount; i++)
#endif
			{
				float black_level, dummyMinZ, dummyMaxZ;
				get_black_level_and_minmaxZ(i, black_level, dummyMinZ, dummyMaxZ);
#ifdef RT_SIDE_LIGHTS
				black_level = 0.1;
#endif
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

	// Shadow Mapping.
	// This block modifies total_shadow_factor.
	// In other words, the shadow casting contribution for both RT and SM ends up in
	// total_shadow_factor.
	float total_shadow_factor = rt_shadow_factor;
	// Don't compute shadow mapping if Raytraced shadows are enabled in the cockpit... it's redundant.
	if (sm_enabled && bRTAllowShadowMapping)
	{
		//float3 P_bias = P + sm_bias * N;
		[loop]
		for (uint i = 0; i < LightCount; i++)
		{
			float shadow_factor = 1.0;
			float black_level, minZ, maxZ;
			get_black_level_and_minmaxZ(i, black_level, minZ, maxZ);
			// Skip lights that won't project black-enough shadows or that are
			// out-of-range for ShadowMapping
			if (black_level > 0.95 || P.z < minZ || P.z > maxZ)
				continue;
			// Apply the same transform we applied to P in ShadowMapVS.hlsl
			float3 Q = mul(lightWorldMatrix[i], float4(P, 1.0)).xyz;
			// Distant objects require more bias, here we're using maxZ as a proxy to tell us
			// how distant is the shadow map and we use that to compensate the bias
			float bias = sm_bias - 1.0 * step(50.0f, maxZ);
			// shadow_factor: 1 -- No shadow
			// shadow_factor: 0 -- Full shadow
			shadow_factor = ShadowMapPCF(i, float3(Q.xy, MetricZToDepth(i, Q.z + bias)), sm_resolution, sm_pcss_samples, sm_pcss_radius);
			// Limit how black the shadows can be to a minimum of black_level
			shadow_factor = max(shadow_factor, black_level);

			// DEBUG
			/*
			if (sm_debug == 1 && shadow_factor < 0.5)
			{
				output.color = float4(0.2, 0.2, 0.5, 1.0);
				return output;
			}
			*/
			// DEBUG

			// Accumulate this light's shadow factor with the total shadow factor
			total_shadow_factor *= shadow_factor;
		}
	}

	//ssdo = ambient + ssdo; // * shadow; // Add the ambient component 
	//ssdo = lerp(ssdo, 1, mask);
	//ssdoInd = lerp(ssdoInd, 0, mask);

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
	//if (GLASS_LO <= mask && mask < GLASS_HI)
	if (glass > 0.01)
	{
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
		diffuse = total_shadow_factor * ssdo.x * diff_int * diffuse + ambient;

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
		//ssdoInd = lerp(ssdoInd, 0.0, shadeless);

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
		spec = contactShadow * total_shadow_factor * debug_spec;

		// Avoid harsh transitions (the lines below will also kill glass spec)
		//spec_col = lerp(spec_col, 0.0, shadeless);
		//spec_bloom = lerp(spec_bloom, 0.0, shadeless);

		//color = color * ssdo + ssdoInd + ssdo * spec_col * spec;
		tmp_color += LightColor[i].rgb * saturate(
			color * diffuse +
			global_spec_intensity * spec_col * spec
			/* + diffuse_difference * */ /* color * */ /* ssdoInd */); // diffuse_diff makes it look cartoonish, and mult by color destroys the effect
		//tmp_bloom += /* min(shadow, contactShadow) */ contactShadow * float4(LightIntensity * spec_col * spec_bloom, spec_bloom);
		tmp_bloom += total_shadow_factor * contactShadow * float4(LightIntensity * spec_col * spec_bloom, spec_bloom);
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
		// P is the original point, pos3D has z inverted:
		// pos3D = (P.xy, -P.z)
		// LightPoint already comes with z inverted (-z) from ddraw, so we can subtract LightPoint - pos3D
		// because both points have -z:
		float3 L = LightPoint[i].xyz - pos3D;
		const float Z = -LightPoint[i].z; // Z is positive depth
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
		// calculate the attenuation for directional lights
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
#ifndef PBR_DYN_LIGHTS
		laser_light_sum += depth_attenuation * attenuation * angle_attenuation * diff_val * LightPointColor[i].rgb;
#else
		// add everything up, PBR version
		const float metallicity	= 0.25;
		const float glossiness	= 0.75;
		const float reflectance	= 0.40;
		//const float ambient = 0.05;
		float3 eye_vec = normalize(-P);
		float3 L_PBR = L;
		float3 N_PBR = N;
		N_PBR.xy = -N_PBR.xy;
		L_PBR.xy = -L_PBR.xy;
		float3 col = addPBR(P, N_PBR, N_PBR, -eye_vec,
			color.rgb, L_PBR, float4(LightPointColor[i].rgb, 1),
			metallicity,
			glossiness, // Glossiness: 0 matte, 1 glossy/glass
			reflectance,
			ambient,
			1.0 // shadow factor
		);
		laser_light_sum += depth_attenuation * attenuation * angle_attenuation * col;
#endif
	}
	//laser_light_sum = laser_light_sum / (laser_light_intensity + laser_light_sum);
	tmp_color += laser_light_intensity * laser_light_sum;
	// Blend the existing tmp_bloom with the new one:
	//laser_light_alpha = saturate(laser_light_alpha);
	//tmp_bloom += float4(laser_light_sum, laser_light_alpha);
	////tmp_bloom.a = max(tmp_bloom.a, laser_light_alpha); // Modifying the alpha fades the bloom too -- not a good idea

	// Reinhard tone mapping:
	if (HDREnabled) {
		//float I = dot(tmp_color, 0.333);
		//tmp_color = lerp(tmp_color, I, I / (I + HDR_white_point)); // whiteout
		//tmp_color = ff_filmic_gamma3(tmp_color);

		//tmp_color = ACESFilm(HDR_white_point * tmp_color);
		//tmp_color = ACES_approx(tmp_color);

		tmp_color = tmp_color / (HDR_white_point + tmp_color);
		//tmp_color = uncharted2_filmic(tmp_color);
		//tmp_color = reinhard_extended(tmp_color, HDR_white_point);
	}
	output.color = float4(sqrt(tmp_color), 1); // Invert gamma correction (approx pow 1/2.2)

#ifdef DISABLED
	//if (ssao_debug == 8)
		//output.color.xyz = bentN.xyz * 0.5 + 0.5;
	if (ssao_debug == 9 || ssao_debug >= 14)
		output.color.xyz = contactShadow;
	//if (ssao_debug == 10)
		//output.color.xyz = bentDiff;
	//if (ssao_debug == 12)
		//output.color.xyz = color * (diff_int * bentDiff + ambient);
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

	output.color = float4(lerp(background, output.color.rgb, blendAlpha), 1);
#ifdef INSTANCED_RENDERING
	output.color = BlendTransparentLayers(output.color, transpColor1, transpColor2);
#else
	output.color = BlendTransparentLayers(output.color, transpColor1, transpColor2, reticleColor);
#endif
	return output;
}
