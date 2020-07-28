// Copyright (c) 2020 Leo Reyes
// Licensed under the MIT license. See LICENSE.txt
// This shader should only be called for destination textures when DC is
// enabled. For regular textures or when the Dynamic Cockpit is disabled,
// use "PixelShader.hlsl" instead.
// This is the holographic version of PixelShaderDC.hlsl
// Noise from https://www.shadertoy.com/view/4sfGzS
// Original VCR effect from ryk: https://www.shadertoy.com/view/ldjGzV
// Stripes from: https://www.shadertoy.com/view/Ms23DR
#include "HSV.h"
#include "shader_common.h"
#include "shading_system.h"
#include "PixelShaderTextureCommon.h"
#include "ShaderToyDefs.h"
#include "ShadertoyCBuffer.h"

// This color should be specified by the current HUD color
static const float4 BG_COLOR     = float4(0.1, 0.1, 0.5, 1.0);
static const float4 BORDER_COLOR = float4(0.4, 0.4, 0.9, 1.0);
static const float4 BLACK        = 0.0;

// HUD offscreen buffer
Texture2D    texture1 : register(t1);
SamplerState sampler1 : register(s1);

// HUD text
Texture2D    texture2 : register(t2);
SamplerState sampler2 : register(s2);

// When the Dynamic Cockpit is active:
// texture0 == cover texture and
// texture1 == HUD offscreen buffer
// texture2 == Text buffer

struct PixelShaderInput
{
	float4 pos    : SV_POSITION;
	float4 color  : COLOR0;
	float2 tex    : TEXCOORD0;
	float4 pos3D  : COLOR1;
	float4 normal : NORMAL;
};

struct PixelShaderOutput
{
	float4 color    : SV_TARGET0;
	float4 bloom    : SV_TARGET1;
	float4 pos3D    : SV_TARGET2;
	float4 normal   : SV_TARGET3;
	float4 ssaoMask : SV_TARGET4;
	float4 ssMask   : SV_TARGET5;
};

// DCPixelShaderCBuffer
cbuffer ConstantBuffer : register(b1)
{
	float4 src[MAX_DC_COORDS_PER_TEXTURE];		  // HLSL packs each element in an array in its own 4-vector (16 bytes) slot, so .xy is src0 and .zw is src1
	float4 dst[MAX_DC_COORDS_PER_TEXTURE];
	uint4 bgColor[MAX_DC_COORDS_PER_TEXTURE / 4]; // Background colors to use for the dynamic cockpit, this divide by 4 is because HLSL packs each elem in a 4-vector,
												  // So each elem here is actually 4 bgColors.

	float ct_brightness;				  // Cover texture brightness. In 32-bit mode the cover textures have to be dimmed.
	float dc_brightness;				  // DC element brightness
	float unused2, unused3;
};

// Noise from https://www.shadertoy.com/view/4sfGzS
float hash(vec3 p)  // replace this by something better
{
	p = fract(p*0.3183099 + .1);
	p *= 17.0;
	return fract(p.x*p.y*p.z*(p.x + p.y + p.z));
}

float noise(in vec3 x)
{
	vec3 i = floor(x);
	vec3 f = fract(x);
	f = f * f*(3.0 - 2.0*f);

	return mix(mix(mix(hash(i + vec3(0, 0, 0)),
		hash(i + vec3(1, 0, 0)), f.x),
		mix(hash(i + vec3(0, 1, 0)),
			hash(i + vec3(1, 1, 0)), f.x), f.y),
		mix(mix(hash(i + vec3(0, 0, 1)),
			hash(i + vec3(1, 0, 1)), f.x),
			mix(hash(i + vec3(0, 1, 1)),
				hash(i + vec3(1, 1, 1)), f.x), f.y), f.z);
}

inline float noise2(in vec2 v) {
	vec3 x = 150.0 * vec3(v, iTime);
	return noise(x);
}

float4 uintColorToFloat4(uint color, out float intensity, out float text_alpha_override, out float obj_alpha_override) {
	float4 result = float4(
		((color >> 16) & 0xFF) / 255.0,  // R 0xFF0000
		((color >> 8) & 0xFF) / 255.0,  // G 0x00FF00
		(color & 0xFF) / 255.0,  // B 0x0000FF
		1); // Alpha
// The alpha component encodes more information:
// bits 0-1: an integer in the range 0..3 that specifies the intensity. intensity = bits[0..1] + 1
// bit 2: Enable/Disable text layer (on/off switch)
//intensity = ((color >> 24) & 0xFF) / 64.0;
	uint temp = (color >> 24) & 0xFF;
	intensity = (temp & 0x03) + 1.0;
	text_alpha_override = (float)((temp & 0x04) >> 2);
	obj_alpha_override = (float)((temp & 0x08) >> 3);
	return result;
}

uint getBGColor(uint i) {
	uint idx = i / 4;
	uint sub_idx = i % 4;
	return bgColor[idx][sub_idx];
}

float sdBox(in float2 p, in float2 b)
{
	float2 d = abs(p) - b;
	return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0);
}

float4 hologram(float2 p)
{
	// Compute the background color
	//float4 hud_texelColor = uintColorToFloat4(getBGColor(0), intensity, text_alpha_override, obj_alpha_override);
	//float4 hud_texelColor = BG_COLOR;
	float d = sdBox(p - 0.5, 0.32 * 1.0);

	// Make the edges round:
	//float din = d - 0.02;
	
	//float dout = d - 0.1;
	float dout = d - 0.07; // Smaller blackout margin
	//float dout = d - 0.03; // Even smaller blackout margin

	//din = smoothstep(0.0, 0.1, din);
	//dout = smoothstep(0.0, 0.1, dout);
	//float4 bgColor = lerp(BG_COLOR, BORDER_COLOR, din);
	//bgColor.a = lerp(bgColor.a, 0.0, dout);
	
	float4 bgColor = BG_COLOR;
	dout = smoothstep(0.0, 0.1, dout);
	
	// Apply scanline noise to the background
	float scans = saturate(2.5 + 5.0 * sin(25.0*iTime + p.y*250.0));
	bgColor *= scans;

	//float scans = 0.5 + 3.0 * sin(25.0*iTime + p.y*250.0);
	//bgColor.rgb *= scans;
	//bgColor.a *= saturate(scans);

	bgColor.a = lerp(bgColor.a, 0.0, dout);
	return bgColor;
}

//*****************************************************************
// CRT/VCR distortion effect
float onOff(float a, float b, float c)
{
	return step(c, sin(iTime + a * cos(iTime*b)));
}

float ramp(float y, float start, float end)
{
	float inside = step(start, y) - step(end, y);
	float fact = (y - start) / (end - start)*inside;
	return (1.0 - fact) * inside;
}

float stripes(vec2 uv)
{

	float n = noise2(uv*vec2(0.5, 1.0) + vec2(1.0, 3.0));
	return n * ramp(mod(uv.y*4. + iTime / 2.0 + sin(iTime + sin(iTime*0.63)), 1.), 0.5, 0.6);
}

float4 getVideo(vec2 uv, out vec2 look)
{
	look = uv;
	float window = 1.0 / (1.0 + 20.0*(look.y - mod(iTime / 4.0, 1.0))*(look.y - mod(iTime / 4.0, 1.0)));
	look.x = look.x + 0.6 * sin(look.y*10.0 + iTime) / 50.0 * onOff(4., 4., .3) * (1.0 + cos(iTime*80.))*window;
	//float vShift = 0.4*onOff(2.,3.,.9)*(sin(iTime)*sin(iTime*20.) + 
	//									 (0.5 + 0.1*sin(iTime*200.)*cos(iTime)));

	//look.y = mod(look.y + vShift, 1.);
	float4 video = hologram(look);
	return video;
}

//*****************************************************************

PixelShaderOutput main(PixelShaderInput input)
{
	PixelShaderOutput output;
	// Zero-out the bloom mask.
	output.bloom = float4(0, 0, 0, 0);
	output.color = 0;
	output.pos3D = 0;
	output.normal = 0; // Holograms are shadeless

	// Fix UVs that are greater than 1 or negative.
	input.tex = frac(input.tex);
	if (input.tex.x < 0.0) input.tex.x += 1.0;
	if (input.tex.y < 0.0) input.tex.y += 1.0;

	//float4 bgColor = hologram(input.tex.xy);
	float2 distorted_uv;
	float4 bgColor = getVideo(input.tex.xy, distorted_uv);
	// Apply noise to the hologram
	bgColor.rgb *= 2.0 * (0.5 + 0.5 * noise2(input.tex.xy));

	float holo_alpha = bgColor.a;

	//output.ssaoMask.r = PLASTIC_MAT;
	//output.ssaoMask.g = DEFAULT_GLOSSINESS; // Default glossiness
	//output.ssaoMask.b = DEFAULT_SPEC_INT;   // Default spec intensity
	//output.ssaoMask.a = 0.0;
	// The material is Plastic because that's material 0. If we set it to SHADELESS_MAT,
	// that's 0.75, so this material will get blended from 0.75 down 0.0, making a hard
	// edge as the material properties change. Blending non-plastic materials makes no
	// sense. Using a plastic material, we avoid blending the material and keep the soft
	// edges -- we just need to kill all the glossiness.
	output.ssaoMask = float4(PLASTIC_MAT, 0.0, 0.0, 0.0 /* coverAlpha */);

	// SS Mask: Normal Mapping Intensity (overriden), Specular Value, unused
	output.ssMask = float4(0.0, 0.0, 0.0, 0.0);

	output.color      = bgColor;
	output.bloom	  = 0.15 * bgColor;
	output.bloom.a    = holo_alpha;
	output.ssaoMask.a = holo_alpha;
	output.ssMask.a   = holo_alpha;

	// Render the captured Dynamic Cockpit buffer into the cockpit destination textures. 
	// We assume this shader will be called iff DynCockpitSlots > 0

	// DEBUG: Display uvs as colors. Some meshes have UVs beyond the range [0..1]
		//output.color = float4(frac(input.tex.xy), 0, 1); // DEBUG: Display the uvs as colors
		//output.ssaoMask = float4(SHADELESS_MAT, 0, 0, 1);
		//output.ssMask = 0;
		//return output;
	// DEBUG
		//return 0.7*hud_texelColor + 0.3*texelColor; // DEBUG DEBUG DEBUG!!! Remove this later! This helps position the elements easily

	// HLSL packs each element in an array in its own 4-vector (16-byte) row. So src[0].xy is the
	// upper-left corner of the box and src[0].zw is the lower-right corner. The same applies to
	// dst uv coords
	
	float intensity = 1.0, text_alpha_override = 1.0, obj_alpha_override = 1.0;
	float4 hud_texelColor = bgColor;

	//[unroll] unroll or loop?
	[loop]
	for (uint i = 0; i < DynCockpitSlots; i++) {
		float2 delta = dst[i].zw - dst[i].xy;
		//float2 s = (input.tex - dst[i].xy) / delta;
		float2 s = (distorted_uv - dst[i].xy) / delta;
		float2 dyn_uv = lerp(src[i].xy, src[i].zw, s);
		//float4 bgColor = uintColorToFloat4(getBGColor(i), intensity, text_alpha_override, obj_alpha_override);

		if (all(dyn_uv >= src[i].xy) && all(dyn_uv <= src[i].zw))
		{
			// Sample the dynamic cockpit texture:
			hud_texelColor = obj_alpha_override * texture1.Sample(sampler1, dyn_uv);
			// Sample the text texture and fix the alpha:
			float4 texelText = texture2.Sample(sampler2, dyn_uv);
			//float textAlpha = text_alpha_override * saturate(3.25 * dot(0.333, texelText.rgb));
			float textAlpha = text_alpha_override * saturate(10.0 * dot(float3(0.33, 0.5, 0.16), texelText.rgb));
			// Blend the text with the DC buffer
			hud_texelColor.rgb = lerp(hud_texelColor.rgb, texelText.rgb, textAlpha);
			hud_texelColor.w = saturate(dc_brightness * max(hud_texelColor.w, textAlpha));
			hud_texelColor = saturate(intensity * hud_texelColor);
			// We can make the text shadeless so that it's easier to read.
			output.ssaoMask.r = lerp(output.ssaoMask.r, SHADELESS_MAT, textAlpha);
			if (textAlpha > 0.7) output.bloom = 0.75 * float4(texelText.rgb, textAlpha);

			const float hud_alpha = hud_texelColor.w;
			// DEBUG: Display the source UVs
			//const float hud_alpha = 1.0;
			//hud_texelColor = float4(dyn_uv, 0, 1);
			// DEBUG

			// Add the background color to the dynamic cockpit display:
			hud_texelColor = lerp(bgColor, hud_texelColor, hud_alpha);
		}
	}
	// At this point hud_texelColor has the color from the offscreen HUD buffer blended with bgColor

	/*
	// Blend the offscreen buffer HUD texture with the cover texture and go shadeless where transparent.
	// Also go shadeless where the cover texture is bright enough and mark that in the bloom mask.
	if (bUseCoverTexture > 0) {
		// We don't have an alpha overlay texture anymore; but we can fake it by disabling shading
		// on areas with a high lightness value

		// coverColor is the cover_texture right now
		float3 HSV = RGBtoHSV(coverColor.xyz);
		float brightness = ct_brightness;
		// The cover texture is bright enough, go shadeless and make it brighter
		if (HSV.z * coverAlpha >= 0.8) {
			diffuse = 1;
			// Increase the brightness:
			HSV = RGBtoHSV(coverColor.xyz);
			HSV.z *= 1.2;
			coverColor.xyz = HSVtoRGB(HSV);
			output.bloom = float4(fBloomStrength * coverColor.xyz, 1);
			brightness = 1.0;
			output.ssaoMask.r = SHADELESS_MAT;
			output.ssaoMask.ga = 1; // Maximum glossiness on light areas?
			output.ssaoMask.b = 0.15; // Low spec intensity
		}
		// Display the dynamic cockpit texture only where the texture cover is transparent:
		// In 32-bit mode, the cover textures appear brighter, we should probably dim them, 
		// that's what the brightness setting below is for:
		coverColor = lerp(hud_texelColor, brightness * coverColor, coverAlpha);
		output.bloom = lerp(0.0, output.bloom, coverAlpha);
		// The diffuse value will be 1 (shadeless) wherever the cover texture is transparent:
		diffuse = lerp(1.0, diffuse, coverAlpha);
		// ssaoMask: SSAOMask/Material, Glossiness x 128, SpecInt, alpha
		// ssMask: NMIntensity, SpecValue, unused
		// DC areas are shadeless, have high glossiness and low spec intensity
		// if coverAlpha is 1, this is the cover texture
		// if coverAlpha is 0, this is the hole in the cover texture
		output.ssaoMask.rgb = lerp(float3(SHADELESS_MAT, 1.0, 0.15), output.ssaoMask.rgb, coverAlpha);
		output.ssMask.rg = lerp(float2(0.0, 1.0), output.ssMask.rg, coverAlpha); // Normal Mapping intensity, Specular Value
		output.ssaoMask.a = max(output.ssaoMask.a, (1.0 - coverAlpha));
		output.ssMask.a = output.ssaoMask.a; // Already clamped in the previous line
	}
	else 
	*/
	//{
		// Holograms don't use cover textures
		//float4 coverColor = hud_texelColor;
		//float diffuse = 1.0;
		// SSAOMask, Glossiness x 128, Spec_Intensity, alpha
		//output.ssaoMask = float4(SHADELESS_MAT, 1, 0.15, 1);
		//output.ssMask = float4(0.0, 1.0, 0.0, 1.0); // No NM, White Spec Val, unused
	//}
	//output.color = float4(/* diffuse * */ hud_texelColor.rgb, hud_texelColor.w);
	output.color = float4(hud_texelColor.rgb, max(holo_alpha, hud_texelColor.w));
	if (bInHyperspace) output.color.a = 1.0;
	return output;
}
