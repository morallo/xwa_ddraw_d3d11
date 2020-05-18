/*
 * Simple shader to add Headlights + Bloom-Mask + Color Buffer
 * Copyright 2020, Leo Reyes.
 * Licensed under the MIT license. See LICENSE.txt
 *
 * The bloom-mask is *not* the final bloom buffer -- this mask is output by the pixel
 * shader and it will be used later to compute proper bloom. Here we use this mask to
 * disable areas of the SSAO buffer that should be bright.
 */
#include "shader_common.h"
#include "HSV.h"
#include "shading_system.h"
#include "SSAOPSConstantBuffer.h"

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

// The Emission Mask buffer
//Texture2D texEmissionMask : register(t8);
//SamplerState samplerEmissionMask : register(s8);

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
	//const float g_fFocalDist = DEFAULT_FOCAL_DIST;
	//const float g_fFocalDist = 1.0;
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

PixelShaderOutput main(PixelShaderInput input)
{
	PixelShaderOutput output;
	output.color = 0;
	output.bloom = 0;
	output.bent = 0;

	float2 input_uv_sub = input.uv * amplifyFactor;
	//float2 input_uv_sub2 = input.uv * amplifyFactor2;
	float2 input_uv_sub2 = input.uv * amplifyFactor;
	float3 color = texColor.Sample(sampColor, input.uv).xyz;
	float4 Normal = texNormal.Sample(samplerNormal, input.uv);
	float3 pos3D = texPos.Sample(sampPos, input.uv).xyz;
	float3 ssdo = texSSDO.Sample(samplerSSDO, input_uv_sub).rgb;
	float3 ssdoInd = texSSDOInd.Sample(samplerSSDOInd, input_uv_sub2).rgb;
	// Bent normals are supposed to encode the obscurance in their length, so
	// let's enforce that condition by multiplying by the AO component: (I think it's already weighed; but this kind of enhances the effect)
	//float3 bentN         = /* ssdo.y * */ texBent.Sample(samplerBent, input_uv_sub).xyz; // TBV
	float3 ssaoMask = texSSAOMask.Sample(samplerSSAOMask, input.uv).xyz;
	float3 ssMask = texSSMask.Sample(samplerSSMask, input.uv).xyz;
	//float3 emissionMask  = texEmissionMask.Sample(samplerEmissionMask, input_uv_sub).xyz;
	float  mask = ssaoMask.x;
	float  gloss_mask = ssaoMask.y;
	float  spec_int_mask = ssaoMask.z;
	float  diff_int = 1.0;
	float  metallic = mask / METAL_MAT;
	float  nm_int_mask = ssMask.x;
	float  spec_val_mask = ssMask.y;
	//bool   shadeless     = mask > GLASS_LO; // SHADELESS_LO;
	// An area is "shadeless" if it's GLASS_MAT or above
	float  shadeless = saturate((mask - GLASS_LO) / (GLASS_MAT - GLASS_LO)); // Avoid harsh transitions
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

	// Compute shadows
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

	float3 tmp_color = 0.0;
	float4 tmp_bloom = 0.0;
	float contactShadow = 1.0;
	float diffuse = 0.0, bentDiff = 0.0, smoothDiff = 0.0, spec = 0.0;
	float debug_spec = 0.0;
	uint i;
	float3 headLightDir = LightVector[0].xyz; // The direction of the headlights is encoded in the first light vector
	//headLightDir.z = -headLightDir.z;

	// Compute the shading contribution from the headlights
	{
		//float3 L = MainLight.xyz; // Lights come with Z inverted from ddraw, so they expect negative Z values in front of the camera
		// MainLight already comes with z inverted (-z) from ddraw, so we can subtract MainLight - pos3D
		// because both points have -z:
		float3 L = MainLight.xyz - pos3D;
		const float Z = -MainLight.z; // Z is positive depth
		const float distance = length(L);
		L /= distance;
		const float depth_attenuation = smoothstep(MainColor.w, Z, distance); // Fade the intensity of the light so that it's 0 at the maximum distance (MainColor.w)
		//const float angle = dot(float3(0.0, 0.0, -1.0), normalize(pos3D));
		//const float angle = dot(float3(0.0, 0.0, -1.0), -L);
		const float angle = max(dot(headLightDir, L), 0.0);
		float angle_attenuation = smoothstep(headlights_angle_cos, 1.0, angle);
		//float angle_attenuation = 1.0;
		float LightIntensity = depth_attenuation * angle_attenuation * dot(MainColor.rgb, 0.333);

		// diffuse component
		//bentDiff   = max(dot(smoothB, L), 0.0);
		smoothDiff = max(dot(smoothN, headLightDir), 0.0);
		// I know that bentN is already multiplied by ssdo.x above; but I'm
		// multiplying it again here to make the contact shadows more obvious
		//contactShadow  = 1.0 - clamp(smoothDiff - ssdo.x * ssdo.x * bentDiff, 0.0, 1.0); // This works almost perfect
		contactShadow = 1.0 - saturate(smoothDiff - ssdo.x * ssdo.x); // Use SSDO instead of bentDiff --> also good
		// In low-lighting conditions, maybe we can lerp contactShadow with the AO mask?
		// This helps a little bit; but mutes the spec reflections, need to think more about this
		//contactShadow = lerp(ssdo.y, contactShadow * contactShadow, smoothDiff);
		contactShadow *= contactShadow;

		diffuse = max(dot(N, headLightDir), 0.0);
		//diffuse = LightIntensity * ssdo.x * diff_int * diffuse + ambient;
		diffuse = LightIntensity * diff_int * diffuse + ambient;

		// Avoid harsh transitions
		diffuse = lerp(diffuse, 1.0, shadeless);
		contactShadow = lerp(contactShadow, 1.0, shadeless);
		ssdoInd = lerp(ssdoInd, 0.0, shadeless);

		// specular component
		float3 eye_vec = normalize(-pos3D); // normalize(eye - pos3D);
		// reflect expects an incident vector: a vector that goes from the light source to the current point.
		// L goes from the current point to the light vector, so we have to use -L:
		//float3 refl_vec = normalize(reflect(-L, N));
		float3 refl_vec = normalize(reflect(headLightDir, N));
		spec = max(dot(eye_vec, refl_vec), 0.0);

		//const float3 H = normalize(L + eye_vec);
		//spec = max(dot(N, H), 0.0);

		 // We can't have exponent == 0 or we'll see a lot of shading artifacts:
		float exponent = max(global_glossiness * gloss_mask, 0.05);
		float spec_bloom_int = global_spec_bloom_intensity;
		if (GLASS_LO <= mask && mask < GLASS_HI) {
			exponent *= 2.0;
			spec_bloom_int *= 3.0; // Make the glass bloom more
		}
		//float spec_bloom = ssdo.y * spec_int * spec_bloom_int * pow(spec, exponent * bloom_glossiness_mult);
		//spec = ssdo.y * LightInt * spec_int * pow(spec, exponent);
		// TODO REMOVE CONTACT SHADOWS FROM SSDO DIRECT (Probably done already)
		float spec_bloom = contactShadow * spec_int_mask * spec_bloom_int * pow(spec, exponent * global_bloom_glossiness_mult);
		debug_spec = LightIntensity * spec_int_mask * pow(spec, exponent);
		spec = /* min(contactShadow, shadow) */ contactShadow * debug_spec;

		// Avoid harsh transitions (the lines below will also kill glass spec)
		//spec_col = lerp(spec_col, 0.0, shadeless);
		//spec_bloom = lerp(spec_bloom, 0.0, shadeless);

		// The following lines MAY be an alternative to remove spec on shadeless surfaces; keeping glass
		// intact
		//spec_col = mask > SHADELESS_LO ? 0.0 : spec_col;
		//spec_bloom = mask > SHADELESS_LO ? 0.0 : spec_bloom;

		//color = color * ssdo + ssdoInd + ssdo * spec_col * spec;
		tmp_color += MainColor.rgb * saturate(
			color * diffuse +
			LightIntensity * global_spec_intensity * spec_col * spec +
			/* diffuse_difference * */ /* color * */ ssdoInd); // diffuse_diff makes it look cartoonish, and mult by color destroys the effect
			//emissionMask);
		tmp_bloom += /* min(shadow, contactShadow) */ contactShadow * float4(LightIntensity * spec_col * spec_bloom, spec_bloom);
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
	return output;
}
