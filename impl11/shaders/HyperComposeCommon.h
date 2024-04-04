// Copyright Leo Reyes, 2019.
// Licensed under the MIT license. See LICENSE.txt
// This shader draws the cockpit on top of the blurred background during
// post-hyper-exit
#include "HSV.h"
#include "ShaderToyDefs.h"
#include "ShadertoyCBuffer.h"

// The foreground texture (shadertoyBuf)
#ifdef INSTANCED_RENDERING
Texture2DArray fgTex : register(t0);
SamplerState   fgSampler : register(s0);

// The background texture (shadertoyAuxBuf)
Texture2DArray bgTex : register(t1);
SamplerState   bgSampler : register(s1);

// The hyperspace effect texture (offscreenAsInput)
Texture2DArray effectTex : register(t2);
SamplerState   effectSampler : register(s2);
#else
Texture2D    fgTex : register(t0);
SamplerState fgSampler : register(s0);

// The background texture (shadertoyAuxBuf)
Texture2D    bgTex : register(t1);
SamplerState bgSampler : register(s1);

// The hyperspace effect texture (offscreenAsInput)
Texture2D    effectTex : register(t2);
SamplerState effectSampler : register(s2);
#endif

#define interdict_mix twirl

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
	float4 color    : SV_TARGET0;
	float4 bloom    : SV_TARGET1;
};

// The HyperZoom effect probably can't be applied in this shader because it works
// on the full screen. Instead the HyperZoom must be applied SBS or using independent
// buffers

static const float3 blue_col = float3(0.50, 0.50, 1.00);
static const float3 red_col = float3(0.90, 0.15, 0.15);
//#define bloom_strength 2.0

PixelShaderOutput main(PixelShaderInput input)
{
	PixelShaderOutput output;
	output.bloom = 0;
	float bloom  = 0;

#ifdef INSTANCED_RENDERING
	float4 fgColor = fgTex.Sample(fgSampler, float3(input.uv, input.viewId));
	float4 bgColor = bgTex.Sample(bgSampler, float3(input.uv, input.viewId));
	float4 effectColor = effectTex.Sample(effectSampler, float3(input.uv, input.viewId));
#else
	float4 fgColor = fgTex.Sample(fgSampler, input.uv);
	float4 bgColor = bgTex.Sample(bgSampler, input.uv);
	float4 effectColor = effectTex.Sample(effectSampler, input.uv);
#endif

	/*
	// HyperZoom
	// Apply the zoom effect
	if (bBGTextureAvailable) {
		vec2 fragCoord = input.uv * iResolution.xy;
		// Convert pixel coord into uv centered at the origin
		float2 uv = fragCoord / iResolution.xy - scr_center;
		bgColor.rgb = HyperZoom(uv);
		bgColor.a = 1.0;
	}
	// HyperZoom
	*/

	float3 color;
	// The following is a bit of a hack. The foreground texture (the cockpit) gets an
	// alpha value between 0.8 and 0.9 when the hologram is enabled; but I'll be damned
	// if I can fix the stupid blend operation to yield alpha=1 from PixelShaderDCHolo.
	// So, instead, let's multiply the alpha for the foreground by 10 so that we can
	// work around this stupid stupid stupid (stupid) problem.
	//float fg_alpha = saturate(10.0 * fgColor.a);
	// Fast-forward several months into the future, and the stupid stupid stupid (stupid) hack
	// above appears to have been resolved finally. The fix is really in the DestBlendAlpha/BlendOpAlpha
	// settings in XwaD3dRenderHook and how they control blending between semi-transparent surfaces like
	// the cockpit glass and the holograms. The following appears to work fine now:
	float lightness = dot(0.333, effectColor.rgb);
	// Combine the background with the hyperspace effect
	if (hyperspace_phase == 2)
		// Replace the background with the tunnel
		color = effectColor.rgb;
	else
		// Blend the trails with the background
		color = lerp(bgColor.rgb, effectColor.rgb, lightness);

	// We don't combine the foreground (cockpit) with the background anymore in
	// this shader. That's taken care of in the DeferredPass() now. Here, we're
	// only interested in the alpha channel of the foreground so that we know
	// where bloom can be applied
	output.color = float4(color, 1.0);

	// Fix the bloom
	if (hyperspace_phase == 1 || hyperspace_phase == 3 || hyperspace_phase == 4)
		bloom = lightness;
	else
		bloom = 0.65 * smoothstep(0.55, 1.0, lightness);

	// Render red streaks during an interdiction
	// TODO: Lerp the color, but only when entering the hypertunnel
	float3 bloom_col = lerp(blue_col, red_col, interdict_mix);
	output.bloom = float4(bloom_strength * bloom_col * bloom, bloom);

	output.bloom *= 1.0 - fgColor.a; // Hide the bloom mask wherever the foreground is solid
	return output;
}
