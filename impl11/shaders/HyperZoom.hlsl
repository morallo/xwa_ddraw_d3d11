#include "ShaderToyDefs.h"

// This is the length of the effect in seconds: (used to be named "t2")
static const float t2 = 2.0;
// This is the time for the hyperzoom effect:
static const float t2_zoom = 1.5;
// This is the overlap between the exit trails and the zoom:
static const float t_overlap = 1.5;

// The background texture (shadertoyAuxBuf)
Texture2D    bgTex     : register(t0);
SamplerState bgSampler : register(s0);

struct PixelShaderInput
{
	float4 pos    : SV_POSITION;
	float4 color  : COLOR0;
	float2 uv     : TEXCOORD0;
	float4 pos3D  : COLOR1;
};

struct PixelShaderOutput
{
	float4 color    : SV_TARGET0;
};

// Distort effect based on: https://stackoverflow.com/questions/6030814/add-fisheye-effect-to-images-at-runtime-using-opengl-es
vec3 distort(in vec2 uv, in float distortion,
	in float dist_apply, in float fade_apply,
	in float2 scr_center, in float x0, in float x1)
{
	vec2 proj_center = scr_center; // -vec2(0, y_center);
	vec2 uv_scaled = uv - proj_center;
	float r = length(uv_scaled);
	float fade_t = 0.25, fade = 1.0;
	vec2 dist_uv = uv_scaled * r * distortion;

	uv_scaled = mix(uv_scaled, dist_uv, dist_apply) + proj_center;

	if (uv_scaled.x < x0  || uv_scaled.x > x1 ||
		uv_scaled.y < 0.0 || uv_scaled.y > 1.0)
		return 0.0;

	vec3 col = bgTex.SampleLevel(bgSampler, uv_scaled, 0).rgb;
	if (r > fade_t) {
		fade = 1.0 - (min(1.0, r) - fade_t) / (1.0 - fade_t);
		fade *= fade;
		fade = mix(1.0, fade, fade_apply);
	}
	return fade * col;
}

float3 HyperZoom(in float2 uv, in float2 scr_center, in float x0, in float x1) {
	vec3  col = 0.0;
	vec3  res = 0.0;
	uint  index;

	float dist_apply[7], dist[7];
	float factors[7], fade[7], d_scale[7];
	int iters[7];
	factors[0] = 40.0; dist[0] = 5.00; dist_apply[0] = 1.0; fade[0] = 1.0;
	factors[1] = 25.0; dist[1] = 5.00; dist_apply[1] = 1.0; fade[1] = 1.0;
	factors[2] = 8.0;  dist[2] = 5.00; dist_apply[2] = 1.0; fade[2] = 1.0;
	factors[3] = 5.0;  dist[3] = 5.00; dist_apply[3] = 1.0; fade[3] = 1.0;
	factors[4] = 3.0;  dist[4] = 0.25; dist_apply[4] = 0.8; fade[4] = 1.0;
	factors[5] = 1.5;  dist[5] = 0.15; dist_apply[5] = 0.2; fade[5] = 1.0;
	factors[6] = 1.0;  dist[6] = 0.00; dist_apply[6] = 0.0; fade[6] = 0.0;
	d_scale[0] = 1.00; iters[0] = 32;
	d_scale[1] = 1.00; iters[1] = 32;
	d_scale[2] = 0.80; iters[2] = 32;
	d_scale[3] = 0.80; iters[3] = 32;
	d_scale[4] = 0.75; iters[4] = 32;
	d_scale[5] = 0.40; iters[5] = 32;
	d_scale[6] = 0.00; iters[6] = 1;

	float t = min(1.0, max(iTime, 0.0) / t2_zoom); // Normalize time in [0..1]
	t = 1.0 - t; // Reverse time to shrink streaks
	t = (6.0 * t) % 7.0;
	index = floor(t);
	float t1 = frac(t);
	float dist_apply_mix = dist_apply[index], dist_mix = dist[index];
	float factors_mix = factors[index], fade_mix = fade[index], d_scale_mix = d_scale[index];
	// DEBUG
	//index = 4.0;
	// DEBUG
	int iters_mix = iters[index];
	
	if (index < 6) {
		factors_mix		= lerp(factors[index], factors[index + 1], t1);
		dist_mix			= lerp(dist[index], dist[index + 1], t1);
		dist_apply_mix	= lerp(dist_apply[index], dist_apply[index + 1], t1);
		fade_mix			= lerp(fade[index], fade[index + 1], t1);
		iters_mix		= round(lerp(iters[index], iters[index + 1], t1));
		d_scale_mix		= lerp(d_scale[index], d_scale[index + 1], t1);
	}

	vec2  d = -uv / float(iters_mix) * d_scale_mix;
	//return float3(uv, 0.0);

	[loop]
	for (int i = 0; i < iters_mix; i++)
	{
		vec2 uv_scaled = uv * factors_mix + scr_center;
		res = distort(uv_scaled, dist_mix, dist_apply_mix, fade_mix, 
					  scr_center, x0, x1);
		col += res;
		uv += d;
	}
	col = 1.2 * col / float(iters_mix);
	return col;
}

PixelShaderOutput main(PixelShaderInput input)
{
	PixelShaderOutput output;
	float4 bgColor;
	float2 scr_center = float2(0.5, 0.5);
	float2 uv = input.uv;
	float x0 = 0.0, x1 = 1.0;

	if (bDirectSBS) 
	{
		if (uv.x < 0.5) {
			scr_center = vec2(0.25, 0.5);
			x0 = 0.0; x1 = 0.5;
		} else {
			scr_center = vec2(0.75, 0.5);
			x0 = 0.5; x1 = 1.0;
		}
	}
	// Convert pixel coord into uv centered at the origin
	uv -= scr_center;

	output.color = float4(HyperZoom(uv, scr_center, x0, x1), 1.0);
	return output;
}