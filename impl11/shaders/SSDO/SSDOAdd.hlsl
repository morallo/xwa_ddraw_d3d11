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

#define DEFAULT_FOCAL_DIST 2.0
//#define diffuse_intensity 1.0
//#define global_ambient 0.005

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
float3 get_normal_from_color(in float2 uv, in float2 offset, in float nm_intensity, out float diff)
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
	diff = clamp(1.0 - (abs(normal.x) + abs(normal.y)), 0.0, 1.0);
	//diff *= diff;

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

/*
float3 old_shadow_factor(in float3 P) {
	float3 cur_pos = P, occluder, diff;
	float2 cur_uv;
	float3 ray_step = shadow_step_size * LightVector[0].xyz;
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

		if (diff.z > shadow_epsilon /* && diff.z < max_dist */) { // Ignore negative z-diffs: the occluder is behind the ray
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
	float3 pos3D		     = texPos.Sample(sampPos, input.uv).xyz;
	float3 ssdo          = texSSDO.Sample(samplerSSDO, input_uv_sub).rgb;
	float3 ssdoInd       = texSSDOInd.Sample(samplerSSDOInd, input_uv_sub2).rgb;
	// Bent normals are supposed to encode the obscurance in their length, so
	// let's enforce that condition by multiplying by the AO component: (I think it's already weighed; but this kind of enhances the effect)
	float3 bentN         = /* ssdo.y * */ texBent.Sample(samplerBent, input_uv_sub).xyz; // TBV
	float3 ssaoMask      = texSSAOMask.Sample(samplerSSAOMask, input.uv).xyz;
	float3 ssMask        = texSSMask.Sample(samplerSSMask, input.uv).xyz;
	float  mask          = ssaoMask.x;
	float  gloss_mask    = ssaoMask.y;
	float  spec_int_mask = ssaoMask.z;
	float  diff_int      = 1.0;
	bool   shadeless     = mask > GLASS_LO; // SHADELESS_LO;
	float  metallic      = mask / METAL_MAT;
	float  nm_int_mask   = ssMask.x;
	float  spec_val_mask = ssMask.y; 
	float  diffuse_difference = 1.0;
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
	pos3D.z = -pos3D.z;
	//if (pos3D.z > INFINITY_Z1 || mask > 0.9) // the test with INFINITY_Z1 always adds an ugly cutout line
	// we should either fade gradually between INFINITY_Z0 and INFINITY_Z1, or avoid this test completely.
	
	// Normals with w == 0 are not available -- they correspond to things that don't have
	// normals, like the skybox
	//if (mask > 0.9 || Normal.w < 0.01) {
	if (Normal.w < 0.01) { // The skybox gets this alpha value
		output.color = float4(color, 1);
		return output;
	}

	color = color * color; // Gamma correction (approx pow 2.2)
	float3 N = normalize(Normal.xyz);
	const float3 smoothN = N;
	const float3 smoothB = bentN;

	// Compute shadows
	float3 SSAO_Normal = float3(N.xy, -N.z);
	//float m_offset = max(moire_offset, moire_offset * (-pos3D.z * moire_scale));
	//float3 shadow_pos3D = float3(pos3D.xy, -pos3D.z - m_offset);
	// SSAO version:
	float m_offset = moire_offset * (-pos3D.z * moire_scale);
	float3 shadow_pos3D = float3(pos3D.xy, -pos3D.z) + SSAO_Normal * m_offset;
	float shadow = 1;
	if (shadow_enable) shadow = shadow_factor(shadow_pos3D, max_dist * max_dist).x;

	//ssdo = ambient + ssdo; // * shadow; // Add the ambient component 
	//ssdo = lerp(ssdo, 1, mask);
	//ssdoInd = lerp(ssdoInd, 0, mask);

	float2 offset = float2(1.0 / screenSizeX, 1.0 / screenSizeY);
	float3 FakeNormal = 0;
	// Glass, Shadeless and Emission should not have normal mapping:
	if (fn_enable && mask < GLASS_LO) {
		FakeNormal = get_normal_from_color(input.uv, offset, nm_int_mask, diffuse_difference);
		// After the normals have blended, we should restore the length of the bent normal:
		// it should be weighed by AO, which is now in ssdo.y
		bentN = ssdo.y * blend_normals(bentN, FakeNormal);
		N = blend_normals(N, FakeNormal);
		//diffuse_difference = 1.0 - length(N - TempN);
		//diffuse_difference = dot(0.333, N - TempN);
		//diffuse_difference *= diffuse_difference;
		//N = TempN;
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
	[unroll]
	for (uint i = 0; i < 1; i++) {
		//float3 L = normalize(LightVector[i].xyz);
		float3 L = LightVector[i].xyz;
		float LightInt = dot(LightColor[i].rgb, 0.333);

		// diffuse component
		bentDiff   = max(dot(smoothB, L), 0.0);
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

		// TODO: USE MULTIPLE LIGHTS
		if (ssao_debug == 11)
			diffuse = max(dot(bentN, L), 0.0);
		else 
			diffuse = max(dot(N, L), 0.0);
		diffuse = min(shadow, ssdo.x) * diff_int * diffuse + ambient;

		// Default case
		//diffuse = ssdo.x * diff_int * diffuse + ambient; // ORIGINAL
		//diffuse = diff_int * diffuse + ambient;

		//diffuse = lerp(diffuse, 1, mask); // This applies the shadeless material; but it's now defined differently
		if (shadeless) {
			diffuse = 1.0;
			contactShadow = 1.0;
			ssdoInd = 0.0;
		}

		// specular component
		//float3 spec_col = lerp(min(6.0 * color, 1.0), 1.0, mask); // Force spec_col to be white on masked (DC) areas
		float3 eye_vec = normalize(-pos3D); // normalize(eye - pos3D);
		// reflect expects an incident vector: a vector that goes from the light source to the current point.
		// L goes from the current point to the light vector, so we have to use -L:
		float3 refl_vec = normalize(reflect(-L, N));
		spec = max(dot(eye_vec, refl_vec), 0.0);

		//float3 viewDir    = normalize(pos3D);
		//float3 halfwayDir = normalize(L + viewDir);
		//float spec = max(dot(N, halfwayDir), 0.0);

		float exponent = global_glossiness * gloss_mask;
		float spec_bloom_int = global_spec_bloom_intensity;
		if (GLASS_LO <= mask && mask < GLASS_HI) {
			exponent *= 2.0;
			spec_bloom_int *= 3.0; // Make the glass bloom more
		}
		//float spec_bloom = ssdo.y * spec_int * spec_bloom_int * pow(spec, exponent * bloom_glossiness_mult);
		//spec = ssdo.y * LightInt * spec_int * pow(spec, exponent);
		// TODO REMOVE CONTACT SHADOWS FROM SSDO DIRECT
		float spec_bloom = contactShadow * spec_int_mask * spec_bloom_int * pow(spec, exponent * global_bloom_glossiness_mult);
		debug_spec = LightInt * spec_int_mask * pow(spec, exponent);
		spec = min(contactShadow, shadow) * debug_spec;

		//color = color * ssdo + ssdoInd + ssdo * spec_col * spec;
		tmp_color += LightColor[i].rgb * saturate(
			color * diffuse + 
			global_spec_intensity * spec_col * spec + 
			/* diffuse_difference * */ /* color * */ ssdoInd); // diffuse_diff makes it look cartoonish, and mult by color destroys the effect
		tmp_bloom += min(shadow, contactShadow) * float4(LightInt * spec_col * spec_bloom, spec_bloom);
	}
	output.color = float4(sqrt(tmp_color), 1); // Invert gamma correction (approx pow 1/2.2)
	output.bloom = tmp_bloom;

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
	if (ssao_debug == 22)
		output.color.xyz = shadow;
	if (ssao_debug == 23)
		output.color.xyz = ssdoInd; // Should display the mask instead of the ssdoInd color
	if (ssao_debug == 24)
		output.color.xyz = ssdoInd;
	if (ssao_debug == 25)
		output.color.xyz = ssdoInd; // Should display a debug value coming from SSDOInd
	if (ssao_debug == 26)
		output.color.xyz = 0.5 * ssdoInd;

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
