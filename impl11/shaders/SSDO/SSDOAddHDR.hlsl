/*
 * Simple shader to add SSAO + Bloom-Mask + Color Buffer
 * HDR version
 * Copyright 2019, Leo Reyes.
 * Licensed under the MIT license. See LICENSE.txt
 *
 * The bloom-mask is *not* the final bloom buffer -- this mask is output by the pixel
 * shader and it will be used later to compute proper bloom. Here we use this mask to
 * disable areas of the SSAO buffer that should be bright.
 */
#include "..\HSV.h"

 // The color buffer
Texture2D texColor : register(t0);
SamplerState sampColor : register(s0);

// The bloom mask buffer
Texture2D texBloom : register(t1);
SamplerState samplerBloom : register(s1);

// The bent normal buffer -- This is the SSDO buffer for LDR rendering
Texture2D texBent : register(t2);
SamplerState samplerBent : register(s2);

// The SSDO Indirect buffer
Texture2D texSSDOInd : register(t3);
SamplerState samplerSSDOInd : register(s3);

// The SSAO mask
Texture2D texSSAOMask : register(t4);
SamplerState samplerSSAOMask : register(s4);

Texture2D texPos : register(t5);
SamplerState sampPos : register(s5);

// The Normals buffer
//Texture2D texNormal : register(t6);
//SamplerState samplerNormal : register(s6);

#define INFINITY_Z 20000

// We're reusing the same constant buffer used to blur bloom; but here
// we really only use the amplifyFactor to upscale the SSAO buffer (if
// it was rendered at half the resolution, for instance)
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
	float white_point, shadow_step, shadow_length, ssao_unused1;
	// 144 bytes
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

float3 blend_normals(float3 n1, float3 n2)
{
	//return normalize(float3(n1.xy*n2.z + n2.xy*n1.z, n1.z*n2.z));
	n1 += float3(0, 0, 1);
	n2 *= float3(-1, -1, 1);
	return n1 * dot(n1, n2) / n1.z - n2;
}

/*
 * From:
 * https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
 * (free to use under public domain CC0 or MIT license)
 */
inline float3 ACESFilm(float3 x)
{
	float a = 2.51f;
	float b = 0.03f;
	float c = 2.43f;
	float d = 0.59f;
	float e = 0.14f;
	return saturate((x*(a*x + b)) / (x*(c*x + d) + e));
}

inline float Reinhard4(float Lin, float Lwhite_sqr) {
	return Lin * (1 + Lin / Lwhite_sqr) / (1 + Lin);
}

inline float3 Reinhard4b(float3 Lin, float Lwhite_sqr) {
	return Lin * (1 + Lin / Lwhite_sqr) / (1 + Lin);
}

inline float3 ReinhardFull(float3 rgb, float Lwhite_sqr) {
	//float3 hsv = RGBtoHSV(rgb);
	//hsv.z = Reinhard4(hsv.z, Lwhite_sqr);
	//return HSVtoRGB(hsv);
	return Reinhard4b(rgb, Lwhite_sqr);
}

float3 ToneMapFilmic_Hejl2015(float3 hdr, float whitePt)
{
	float4 vh = float4(hdr, whitePt);
	float4 va = (1.425 * vh) + 0.05f;
	float4 vf = ((vh * va + 0.004f) / ((vh * (va + 0.55f) + 0.0491f))) - 0.0821f;
	return vf.rgb / vf.www;
}

float4 main(PixelShaderInput input) : SV_TARGET
{
	float2 input_uv_sub = input.uv * amplifyFactor;
	//float2 input_uv_sub2 = input.uv * amplifyFactor2;
	float2 input_uv_sub2 = input.uv * amplifyFactor;
	float3 albedo = texColor.Sample(sampColor, input.uv).xyz;
	float3 bentN = texBent.Sample(samplerBent, input_uv_sub).xyz; // Looks like bentN is already normalized
	float3 pos3D = texPos.Sample(sampPos, input.uv).xyz;
	//float3 Normal = texNormal.Sample(samplerNormal, input.uv).xyz;
	float4 bloom = texBloom.Sample(samplerBloom, input.uv);
	//float3 ssdo = texSSDO.Sample(samplerSSDO, input_uv_sub).rgb; // HDR
	float3 ssdoInd = texSSDOInd.Sample(samplerSSDOInd, input_uv_sub2).rgb;
	float3 ssaoMask = texSSAOMask.Sample(samplerSSAOMask, input.uv).xyz;
	//float  mask = max(dot(0.333, bloom.xyz), dot(0.333, ssaoMask));
	float mask = dot(0.333, ssaoMask);

	// Early exit: don't touch the background
	if (pos3D.z > INFINITY_Z) return float4(albedo, 1);

	// Apply Normal Mapping
	if (fn_enable) {
		float2 offset = float2(1 / screenSizeX, 1 / screenSizeY);
		float nm_intensity = lerp(nm_intensity_near, nm_intensity_far, saturate(pos3D.z / 4000.0));
		float3 FakeNormal = get_normal_from_color(input.uv, offset);
		bentN = blend_normals(nm_intensity * FakeNormal, bentN);
	}

	float3 temp = ambient;
	temp += LightColor.rgb  * saturate(dot(bentN,  LightVector.xyz));
	temp += invLightColor   * saturate(dot(bentN, -LightVector.xyz));
	temp += LightColor2.rgb * saturate(dot(bentN,  LightVector2.xyz));
	float3 color = albedo * temp;
	// Apply tone-mapping:
	//color = color / (color + 1);
	
	//color = ReinhardFull(color, gamma*gamma);
	//color = ToneMapFilmic_Hejl2015(color, gamma);
	color = ACESFilm(color);
	color = pow(abs(color), 1.0 / gamma);
	color = lerp(color, albedo, mask * 0.75);
	
	return float4(color, 1);
	//ssdo = ambient + ssdo; // Add the ambient component
	// Apply tone mapping:
	//ssdo = ssdo / (ssdo + 1);
	//ssdo = pow(abs(ssdo), 1.0 / gamma);
	//return float4(ssdo, 1);
	//float3 hsv = RGBtoHSV(color);
	//float level = dot(0.333, ssdo);
	//hsv.z = saturate(level);
	//color = HSVtoRGB(hsv);
	//return float4(ssdo, 1);

	/*
	
	if (level >= 0.5)
		//color = 1 - (1 - color) * (1 - ssdo);
		color = 1 - 2 * (1 - color) * (1 - ssdo);
	else
		//color = color * ssdo;
		color = 2 * color * ssdo;
	*/

	//ssdo = lerp(ssdo, 1, mask);
	//ssdoInd = lerp(ssdoInd, 0, mask);
	//color = color * ssdo + ssdoInd;
	//return float4(color, 1);
}
