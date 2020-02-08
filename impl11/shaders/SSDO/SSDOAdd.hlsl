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

// The (Flat) Normals buffer
Texture2D texNormal : register(t5);
SamplerState samplerNormal : register(s5);

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
	float shadow_k, ssao_unused0, ssao_unused1;
	// 176 bytes
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
float3 get_normal_from_color(float2 uv, float2 offset)
{
	float3 offset_swiz = float3(offset.xy, 0);
	// Luminosity samples
	float hpx = dot(texColor.SampleLevel(sampColor, float2(uv + offset_swiz.xz), 0).xyz, 0.333) * fn_scale;
	float hmx = dot(texColor.SampleLevel(sampColor, float2(uv - offset_swiz.xz), 0).xyz, 0.333) * fn_scale;
	float hpy = dot(texColor.SampleLevel(sampColor, float2(uv + offset_swiz.zy), 0).xyz, 0.333) * fn_scale;
	float hmy = dot(texColor.SampleLevel(sampColor, float2(uv - offset_swiz.zy), 0).xyz, 0.333) * fn_scale;

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
}

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
	//P.xy -= float2(-0.5, 0.5); // Is this wrong? I think the y axis is inverted, so adding 0.5 would be fine (?)
	P.xy -= float2(-1.0, 1.0); // DirectSBS may be different
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

PixelShaderOutput main(PixelShaderInput input)
{
	PixelShaderOutput output;
	output.color = 0;
	output.bloom = 0;
	output.bent  = 0;

	// DEBUG
	//output.color = float4(input.uv.xy, 0, 1);
	//return output;
	// DEBUG

	float2 input_uv_sub = input.uv * amplifyFactor;
	//float2 input_uv_sub2 = input.uv * amplifyFactor2;
	float2 input_uv_sub2 = input.uv * amplifyFactor;
	float3 color     = texColor.Sample(sampColor, input.uv).xyz;
	//float3 bentN    = texBent.Sample(samplerBent, input_uv_sub).xyz;
	float4 Normal    = texNormal.Sample(samplerNormal, input.uv);
	float3 pos3D		 = texPos.Sample(sampPos, input.uv).xyz;
	float3 ssdo      = texSSDO.Sample(samplerSSDO, input_uv_sub).rgb;
	float3 ssdoInd   = texSSDOInd.Sample(samplerSSDOInd, input_uv_sub2).rgb;
	float3 ssaoMask  = texSSAOMask.Sample(samplerSSAOMask, input.uv).xyz;
	//float  Navg     = dot(0.333, Normal.xyz);
	float  mask      = ssaoMask.x; // dot(0.333, ssaoMask);
	float  gloss     = ssaoMask.y;
	float  spec_int  = ssaoMask.z;
	float  diff_int  = 1.0;
	bool   shadeless = mask > SHADELESS_LO;
	float  metallic  = mask / METAL_MAT;
	
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
	float3 L = normalize(LightVector.xyz);
	float3 N = normalize(Normal.xyz);

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

	//ssdo = ambient + ssdo; // * shadow; // Add the ambient component 
	//ssdo = lerp(ssdo, 1, mask);
	//ssdoInd = lerp(ssdoInd, 0, mask);

	float2 offset = float2(1.0 / screenSizeX, 1.0 / screenSizeY);
	float3 FakeNormal = 0;
	// Glass, Shadeless and Emission should not have normal mapping:
	if (fn_enable && mask < GLASS_LO) {
		FakeNormal = get_normal_from_color(input.uv, offset);
		N = blend_normals(N, FakeNormal);
	}
	output.bent = float4(N * 0.5 + 0.5, 1); // DEBUG PURPOSES ONLY

	// Specular color
	float3 spec_col = 1.0;
	float3 HSV = RGBtoHSV(color);
	// Handle both plastic and metallic materials
	if (mask < METAL_HI) {
		// The tint varies from 0 for plastic materials to 1 for fully metallic mats
		float tint = lerp(0.0, 1.0, metallic);
		diff_int = lerp(1.0, 0.0, metallic); // Purely metallic surfaces have no diffuse component (?)
		HSV.y *= tint * saturation_boost;
		HSV.z *= lightness_boost;
		spec_col = HSVtoRGB(HSV);
	}

	// diffuse component
	float diffuse = max(dot(N, L), 0.0);
	if (ss_debug == 1) {
		output.color.xyz = diffuse + ambient;
		output.color.a = 1.0;
		return output;
	}
	else if (ss_debug == 2)
		//diffuse = ssdo.x * diff_int * diffuse + ambient;
		diffuse = diff_int * min(ssdo.x, diffuse);
	else if (ss_debug == 3)
		diffuse *= ssdo.x;
	else
		diffuse = diff_int * diffuse + ambient;
		
	//diffuse = lerp(diffuse, 1, mask); // This applies the shadeless material; but it's now defined differently
	diffuse = shadeless ? 1.0 : diffuse;
	
	// specular component
	//float3 eye = 0.0;
	//float3 spec_col = lerp(min(6.0 * color, 1.0), 1.0, mask); // Force spec_col to be white on masked (DC) areas
	//float3 spec_col = /* ssdo.x * */ spec_int;
	//float3 spec_col = 0.35;
	float3 eye_vec  = normalize(-pos3D); // normalize(eye - pos3D);
	// reflect expects an incident vector: a vector that goes from the light source to the current point.
	// L goes from the current point to the light vector, so we have to use -L:
	float3 refl_vec = normalize(reflect(-L, N));
	float  spec     = max(dot(eye_vec, refl_vec), 0.0);
	
	//float3 viewDir    = normalize(pos3D);
	//float3 halfwayDir = normalize(L + viewDir);
	//float spec = max(dot(N, halfwayDir), 0.0);

	//float  exponent = 10.0;
	float exponent = glossiness * gloss;
	if (GLASS_LO <= mask && mask < GLASS_HI)
		exponent *= 2.0;
	float spec_bloom = spec_int * spec_bloom_intensity * pow(spec, exponent * bloom_glossiness_mult);
	spec = spec_int * pow(spec, exponent);

	//color = color * ssdo + ssdoInd + ssdo * spec_col * spec;
	color = LightColor.rgb * (color * diffuse + spec_intensity * spec_col * spec); // +ambient;
	output.color = float4(sqrt(color), 1); // Invert gamma correction (approx pow 1/2.2)
	//output.bloom = float4(spec_col * ssdo * spec_bloom, spec_bloom);
	output.bloom = float4(spec_col * spec_bloom, spec_bloom);
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
