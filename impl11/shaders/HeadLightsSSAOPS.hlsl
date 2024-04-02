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

// The SSAO buffer
Texture2D texSSAO : register(t1);
SamplerState samplerSSAO : register(s1);

// The Background buffer
Texture2D texBackground : register(t2);

// The SSAO mask
Texture2D texSSAOMask : register(t3);
SamplerState samplerSSAOMask : register(s3);


// The position buffer (linear X,Y,Z)
Texture2D    texPos   : register(t4);
SamplerState sampPos  : register(s4);

// The Normals buffer
Texture2D texNormal : register(t5);
SamplerState samplerNormal : register(s5);

// The Shading System Mask buffer
Texture2D texSSMask : register(t6);
SamplerState samplerSSMask : register(s6);

// Transparent layer 1
Texture2D transp1 : register(t18);
// Transparent layer 2
Texture2D transp2 : register(t19);

// Reticle (only available in non VR mode)
Texture2D reticleTex : register(t20);


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
	float2 uv : TEXCOORD;
};

struct PixelShaderOutput
{
	float4 color : SV_TARGET0;
	float4 bloom : SV_TARGET1;
};

inline float3 getPosition(in float2 uv, in float level) {
	// The use of SampleLevel fixes the following error:
	// warning X3595: gradient instruction used in a loop with varying iteration
	// This happens because the texture is sampled within an if statement (if FGFlag then...)
	return texPos.SampleLevel(sampPos, uv, level).xyz;
}

// See: https://wickedengine.net/2017/10/22/which-blend-state-for-me/
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

float4 BlendTransparentLayers(
	in float4 color,
	in float4 transpColor1,
	in float4 transpColor2)
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
	float2 input_uv_sub  = input.uv * amplifyFactor;
	float4 texelColor    = texColor.Sample(sampColor, input.uv);
	float4 Normal        = texNormal.Sample(samplerNormal, input.uv);
	float3 ssao          = texSSAO.Sample(samplerSSAO, input_uv_sub).rgb;
	float3 ssaoMask      = texSSAOMask.Sample(samplerSSAOMask, input.uv).xyz;
	float3 ssMask        = texSSMask.Sample(samplerSSMask, input.uv).xyz;
	float3 background    = texBackground.Sample(sampColor, input.uv).rgb;
	float4 transpColor1  = transp1.Sample(sampColor, input.uv);
	float4 transpColor2  = transp2.Sample(sampColor, input.uv);
	float4 reticleColor  = reticleTex.Sample(sampColor, input.uv);
	float3 color         = texelColor.rgb;
	float  mask          = ssaoMask.x;
	float  gloss_mask    = ssaoMask.y;
	float  spec_int_mask = ssaoMask.z;
	float  diff_int      = 1.0;
	float  metallic      = mask / METAL_MAT;
	float  glass         = ssMask.x;
	float  spec_val      = ssMask.y;
	float  shadeless     = ssMask.z;
	float3 pos3D;

	const float blendAlphaSat = saturate(2.0 * Normal.w);
	const float blendAlpha = blendAlphaSat * blendAlphaSat;

	PixelShaderOutput output;
	output.color = 0;
	output.bloom = 0;

	// Normals with w == 0 are not available -- they correspond to things that don't have
	// normals, like the skybox
	/*
	if (Normal.w < 0.001) { // The skybox gets this alpha value
		output.color = float4(color, 1);
		return output;
	}
	*/

	if (shadeless >= 0.95)
	{
		output.color = float4(lerp(background, color, texelColor.a), 1);
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

	pos3D = getPosition(input.uv, 0);

	// Fade shading with distance: works for Yavin, doesn't work for large space missions with planets on them
	// like "Enemy at the Gates"... so maybe enable distance_fade for planetary missions? Those with skydomes...
	float distance_fade = enable_dist_fade * saturate((pos3D.z - INFINITY_Z0) / INFINITY_FADEOUT_RANGE);
	shadeless = saturate(lerp(shadeless, 1.0, distance_fade));

	// This shader needs normal mapping...

	// We need to invert the Z-axis for illumination because the normals are Z+ when viewing the camera
	// so that implies that Z increases towards the viewer and decreases away from the camera.
	// We could also avoid inverting Z in PixelShaderTexture... but then we also need to invert the fake
	// normals.
	pos3D.z = -pos3D.z;

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

	float3 tmp_color = 0.0;
	float4 tmp_bloom = 0.0;
	float3 headLightDir = LightVector[0].xyz; // The direction of the headlights is encoded in the first light vector

	// Compute the shading contribution from the headlights
	{
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
		float diffuse = max(dot(N, headLightDir), 0.0);
		diffuse = LightIntensity * ssao.x * diff_int * diffuse + ambient;
		//diffuse = (diff_max * diff_int * diffuse) + (ssao.x * amb_max * ambient);
		//diffuse = diff_int * diffuse + ambient;

		//diffuse = lerp(diffuse, 1, mask); // This applies the shadeless material; but it's now defined differently
		//diffuse = shadeless ? 1.0 : diffuse;
		diffuse = lerp(diffuse, 1.0, shadeless);

		// specular component
		//float3 eye = 0.0;
		//float3 spec_col = lerp(min(6.0 * color, 1.0), 1.0, mask); // Force spec_col to be white on masked (DC) areas
		//float3 spec_col = 0.35;
		float3 eye_vec = normalize(-pos3D); // normalize(eye - pos3D);
		// reflect expects an incident vector: a vector that goes from the light source to the current point.
		// L goes from the current point to the light vector, so we have to use -L:
		float3 refl_vec = normalize(reflect(headLightDir, N));
		float  spec = max(dot(eye_vec, refl_vec), 0.0);

		//float3 viewDir    = normalize(-pos3D);
		//float3 halfwayDir = normalize(L + viewDir);
		//float spec = max(dot(N, halfwayDir), 0.0);

		float spec_bloom = spec_int_mask * spec_bloom_int * pow(spec, exponent * global_bloom_glossiness_mult);
		spec = LightIntensity * spec_int_mask * pow(spec, exponent);

		//color = color * ssdo + ssdoInd + ssdo * spec_col * spec;
		tmp_color += MainColor.rgb * (color * diffuse + LightIntensity * global_spec_intensity * spec_col * spec);
		tmp_bloom += float4(LightIntensity * spec_col * spec_bloom, spec_bloom);
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
	for (uint i = 0; i < num_lasers; i++)
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
			const float depth_attenuation_A = smoothstep(L_FADEOUT_A_1, L_FADEOUT_A_0, Z); // Fade the cockpit flash quickly
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
	if (HDREnabled) tmp_color = tmp_color / (HDR_white_point + tmp_color);
	output.color = float4(sqrt(tmp_color), 1); // Invert gamma correction (approx pow 1/2.2)

	output.color = float4(lerp(background, output.color.rgb, blendAlpha), 1);

#ifdef INSTANCED_RENDERING
	output.color = BlendTransparentLayers(output.color, transpColor1, transpColor2);
#else
	output.color = BlendTransparentLayers(output.color, transpColor1, transpColor2, reticleColor);
#endif
	return output;
}
