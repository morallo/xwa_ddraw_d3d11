/*
 * Hyperspace streaks
 *
  * You can use it under the terms of the MIT license, see LICENSE.TXT
 * (free to use even in commercial projects, attribution required)
 *
 * Simplified and adapted from:
 * https://www.shadertoy.com/view/MlKBWw
 *
 * Adapted by Leo Reyes for X-Wing Alliance, 2019.
 */

#include "ShaderToyDefs.h"

#define TAU 6.28318

// The Foreground Color Buffer (_shadertoyBuf)
Texture2D fgColorTex : register(t0);
SamplerState fgColorSampler : register(s0);

// The Background Color Buffer (_shadertoyAuxBuf)
Texture2D bgColorTex : register(t1);
SamplerState bgColorSampler : register(s1);

// The way this shader works is by looking at the screen as if it were a disk and then
// this disk is split into a number of slices centered at the origin. Each slice renders
// a single trail. So this setting controls the overall density of the effect:
#define NUM_SLICES 125.0
//#define NUM_SLICES 50.0

// Each trail is rendered within its slice; but to avoid generating regular patterns, we
// randomly offset the trail from the center of the slice by this amount:
static const float MAX_SLICE_OFFSET = 0.4;

// This is the length of the effect in seconds: (used to be named "t2")
static const float t2 = 2.0;
// This is the time for the hyperzoom effect:
static const float t2_zoom = 1.5;
// This is the overlap between the exit trails and the zoom:
static const float t_overlap = 1.5;

// T_JUMP is in normalized [0..1] time: this is the time when the trails zoom out of view
// because we've jumped into hyperspace:
static const float T_JUMP = 0.90;
// This is the speed during the final jump:
static const float jump_speed = 5.0;

// I've noticed that the effect tends to have a bluish tint. In this shader, the blue color
// is towards the start of the trail, and the white color towards the end:
//static const vec3 blue_col = vec3(0.5, 0.7, 1);
static const vec3 blue_col = vec3(0.3, 0.3, 0.6);
static const vec3 white_col = vec3(0.8, 0.8, 0.95);

inline float getTime() {
	// iTime goes from t2 + t2_zoom - overlap (full t_max) to 0
	// This formula returns the segment t2..0, and negative numbers beyond that
	return iTime - t2_zoom + t_overlap;
}

//////////////////////////////////////////////////////////////////
// Hyperzoom effect from https://www.shadertoy.com/view/wdyXW1
//////////////////////////////////////////////////////////////////
// Loosely based on Inigo Quilez's shader: https://www.shadertoy.com/view/4sfGRn
// MIT License
// Inspired by the hyperzoom (when exiting out of hyperspace) seen here:
// https://loiter.co/i/best-hyperspace-scene-in-any-star-wars/

static const vec2 scr_center = vec2(0.5, 0.5);
static const float speed = 1.0; // 100 is super-fast, 50 is fast, 25 is good

// Distort effect based on: https://stackoverflow.com/questions/6030814/add-fisheye-effect-to-images-at-runtime-using-opengl-es
vec3 distort(in vec2 uv, in float distortion,
	in float dist_apply, in float fade_apply)
{
	vec2 uv_scaled = uv - scr_center;
	float r = length(uv_scaled);
	float fade_t = 0.25, fade = 1.0;
	vec2 dist_uv = uv_scaled * r * distortion;

	uv_scaled = mix(uv_scaled, dist_uv, dist_apply) + scr_center;
	
	if (uv_scaled.x < 0.0 || uv_scaled.x > 1.0 ||
		uv_scaled.y < 0.0 || uv_scaled.y > 1.0)
		return 0.0;

	vec3 col = bgColorTex.SampleLevel(bgColorSampler, uv_scaled, 0).rgb;
	if (r > fade_t) {
		fade = 1.0 - (min(1.0, r) - fade_t) / (1.0 - fade_t);
		fade *= fade;
		fade = mix(1.0, fade, fade_apply);
	}
	return fade * col;
}

struct PixelShaderInput
{
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD;
};

struct PixelShaderOutput
{
	float4 color    : SV_TARGET0;
	float4 bloom    : SV_TARGET1;
};

float3 HyperZoom(float2 uv) {
	vec3  col = 0.0;
	vec3  res = 0.0;
	uint  index;

	/*
	Frame: Effect

	0: No effect
	1: Very small
	2: Small; but visible
	3: 1/4 of the screen
	4: 1/2 of the screen
	5: Full screen, blurred
	6: Full screen
	*/

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
	iters[0] = 32; iters[1] = 32; iters[2] = 32;
	iters[3] = 32; iters[4] = 32; iters[5] = 32;
	iters[6] = 1;
	d_scale[0] = 1.00; 
	d_scale[1] = 1.00; 
	d_scale[2] = 0.80;
	d_scale[3] = 0.80; 
	d_scale[4] = 0.75; 
	d_scale[5] = 0.40;
	d_scale[6] = 0.00;

	// The higher the uv multipler, the smaller the image
	//fragColor = texture(iChannel0, uv * 3.0 + scr_center);
	//return;

	//uv  = uv / mod(iTime, 5.0);
	//uv += scr_center;

	float t = min(1.0, max(iTime, 0.0) / t2_zoom); // Normalize time in [0..1]
	t = 1.0 - t; // Reverse time to shrink streaks
	t = (6.0 * t) % 7.0;
	index = floor(t);
	float t1 = frac(t);
	float dist_apply_mix = dist_apply[index], dist_mix = dist[index];
	float factors_mix = factors[index], fade_mix = fade[index], d_scale_mix = d_scale[index];
	int iters_mix = iters[index];
	if (index < 6) {
		factors_mix = lerp(factors[index], factors[index + 1], t1);
		dist_mix = lerp(dist[index], dist[index + 1], t1);
		dist_apply_mix = lerp(dist_apply[index], dist_apply[index + 1], t1);
		fade_mix = lerp(fade[index], fade[index + 1], t1);
		iters_mix = round(lerp(iters[index], iters[index + 1], t1));
		d_scale_mix = lerp(d_scale[index], d_scale[index + 1], t1);
	}
	//index = 5; // DEBUG
	//index = index % 7;

	//vec2  d = -uv / float(iters[index]) * d_scale[index];
	vec2  d = -uv / float(iters_mix) * d_scale_mix;

	//time_d = iTime;
	//time_d = 1.8;

	//d *= 0.75; // Smaller factors = less blur
	[loop]
	for (int i = 0; i < iters_mix; i++)
	//for (int i = 0; i < iters[index]; i++)
	{
		//vec2 uv_scaled = uv * factors[index] + 0.5;
		vec2 uv_scaled = uv * factors_mix + 0.5;
		//res = distort(uv_scaled, dist[index],
		//	dist_apply[index], fade[index]);
		res = distort(uv_scaled, dist_mix,
			dist_apply_mix, fade_mix);
		col += res;
		uv += d;
	}
	//col = 1.2 * col / float(iters[index]);
	col = 1.2 * col / float(iters_mix);
	return col;
}

// Hyperspace Zoom
PixelShaderOutput mainZoom(PixelShaderInput input)
{
	PixelShaderOutput output;
	vec4 fragColor = vec4(0.0, 0.0, 0.0, 1);
	vec2 fragCoord = input.uv * iResolution.xy;
	output.bloom = 0;

	// Early exit: avoid rendering outside the original viewport edges
	if (input.uv.x < x0 || input.uv.x > x1 ||
		input.uv.y < y0 || input.uv.y > y1)
	{
		output.color = 0.0;
		return output;
	}
	
	// Convert pixel coord into uv centered at the origin
	vec2 uv = fragCoord / iResolution.xy - scr_center;
	vec3 col = HyperZoom(uv);
	fragColor = vec4(col, 1.0);

	output.color = fragColor;
	return output;
}

/*
// Hyperspace streaks
PixelShaderOutput main(PixelShaderInput input)
{
	PixelShaderOutput output;
	vec4 fragColor = vec4(0.0, 0.0, 0.0, 1);
	vec2 fragCoord = input.uv * iResolution.xy;
	vec2 uv;
	vec3 streakcol = 0.0;
	float4 fgcol = fgColorTex.SampleLevel(fgColorSampler, input.uv, 0); // Use this texture to mask the bloom effect
	float4 bgcol;
	float bloom = 0.0, white_level;

	//output.pos3D = 0;
	//output.normal = 0;
	//output.ssaoMask = 1;
	output.bloom = 0;

	// Early exit: avoid rendering outside the original viewport edges
	if (input.uv.x < x0 || input.uv.x > x1 ||
		input.uv.y < y0 || input.uv.y > y1)
	{
		output.color = 0.0;
		return output;
	}

	// Render the streaks
	bloom = 0.0;
	for (int i = -1; i <= 1; i++)
		for (int j = -1; j <= 1; j++) {
			streakcol += pixelVal(4.0 * fragCoord + vec2(i, j), white_level);
			bloom += white_level;
		}
	streakcol /= 9.0;
	bloom /= 9.0;
	//bloom = 0.0;
	output.bloom = float4(5.0 * float3(0.5, 0.5, 1) * bloom, bloom);
	output.bloom *= 1.0 - fgcol.a; // Hide the bloom mask wherever the foreground is solid

	// Convert pixel coord into uv centered at the origin
	uv = fragCoord / iResolution.xy - scr_center;
	// Apply the zoom effect
	bgcol = 0.0;
	if (bBGTextureAvailable) bgcol.rgb = HyperZoom(uv);
	bgcol.a = 1.0;

	// Output to screen
	fragColor = vec4(streakcol, 1.0);
	float lightness = dot(0.333, fragColor.rgb);
	// Mix the background color with the streaks
	fragColor = lerp(bgcol, fragColor, lightness);

	output.color = fragColor;
	return output;
}
*/

float sdLine(in vec2 p, in vec2 a, in vec2 b, in float ring)
{
	vec2 pa = p - a, ba = b - a;
	float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
	return length(pa - ba * h) - ring;
}

float rand(vec2 co) {
	return fract(sin(dot(co.xy, vec2(12.9898, 78.233))) * 43758.5453);
}

PixelShaderOutput main(PixelShaderInput input) {
	PixelShaderOutput output;
	vec4 fragColor = vec4(0.0, 0.0, 0.0, 1);
	vec2 fragCoord = input.uv * iResolution.xy;
	vec3 color = 0.0;
	output.color = 0.0;
	output.bloom = 0.0;

	// Early exit: avoid rendering outside the original viewport edges
	if (input.uv.x < x0 || input.uv.x > x1 ||
		input.uv.y < y0 || input.uv.y > y1)
	{
		output.color = 0.0;
		return output;
	}

	float4 fgcol = fgColorTex.Sample(fgColorSampler, input.uv);
	float4 bgcol = bgColorTex.Sample(bgColorSampler, input.uv);
	//float t = mod(iTime, T_MAX) / t2;
	float t = max(0.0, getTime()) / t2;
	t = 1.0 - t; // Reverse time because now this effect expects "regular" forward time
	vec2 p = (2.0 * fragCoord.xy - iResolution.xy) / min(iResolution.x, iResolution.y);
	p += vec2(0, y_center); // In XWA the aiming HUD is not at the screen's center

	float p_len = length(p);
	vec3 v = vec3(p, -1.0);
	v = mul(viewMat, vec4(v, 0.0)).xyz;

	float trail_start, trail_end, trail_length = 0.5;
	// Fade all the trails into view from black to a little above full-white:
	float fade = mix(1.4, 0.0, smoothstep(0.65, 0.95, t));
	float bloom = 0.0;

	// Each slice renders a single trail; but we can render multiple layers of
	// slices to add more density and randomness to the effect:
	for (float i = 0.0; i < 60.0; i++)
	{
		vec3 trail_color = 0.0;
		float angle = atan2(v.y, v.x) / 3.141592 / 2.0 + 0.13 * i;

		float slice = floor(angle * NUM_SLICES);
		float slice_fract = fract(angle * NUM_SLICES);
		// Don't center the trail in the slice: wiggle it a little bit:
		float slice_offset = MAX_SLICE_OFFSET *
			rand(vec2(slice, 4.0 + i * 25.0)) - (MAX_SLICE_OFFSET / 2.0);
		// This is the speed of the current slice
		float fspeed = 1.0 * rand(vec2(slice, 1.0 + i * 0.1)) + i * 0.01;

		// Without dist, all trails get stuck to the walls of the
		// tunnel.
		float dist = rand(vec2(slice, 1.0 + i * 2.0)) * (1.0 + i);
		float z = dist * v.z / length(v.xy);

		trail_end = 2.0 * rand(vec2(slice, i + 10.0));
		// Make half the trails appear behind the camera, so that
		// there's something behind the camera if we look at it:
		if (i > 30.0) trail_end = 4.0 * rand(vec2(slice, i + 10.0)) - 3.5;
		trail_end -= t * fspeed;

		// Adding to the trail pushes it "back": Z+ is into the screen
		// away from the camera
		trail_start = trail_end + trail_length;
		// Shrink the trails into their ends:
		trail_start = max(trail_end,
			trail_start - (t * fspeed) -
			mix(0.0, jump_speed,
				smoothstep(0.5, 1.0, t))
		);

		float trail_x = smoothstep(trail_start, trail_end, z);
		trail_color = mix(blue_col, white_col, trail_x);

		// This line computes the distance from the current pixel, in "slice-coordinates"
		// to the ideal trail centered at the slice center. The last argument makes the lines
		// a bit thicker when they reach the edges as time progresses.
		float h = sdLine(
			vec2(slice_fract + slice_offset, z),
			vec2(0.5, trail_start),
			vec2(0.5, trail_end),
			mix(0.0, 0.015, z));

		// This threshold adds a "glow" to the line. This glow grows with
		// time:
		float threshold = mix(0.12, 0.0, smoothstep(0.5, 0.8, t));
		//float threshold = 0.12;
		h = (h < 0.01) ? 1.0 : 0.75 * smoothstep(threshold, 0.0, abs(h));

		trail_color *= fade * h;
		// DEBUG
		// This part displays the size of each slice.
		//float r = 0.0;
		//if (trail_start <= p_len && p_len <= trail_end)
		//    r = 1.0;
		//trail_color = vec3(h, h, h);
		// DEBUG

		// Accumulate this trail with the previous ones
		color = max(color, trail_color);
		bloom = max(bloom, dot(0.333, trail_color));
	}

	// Whiteout
	color += mix(1.0, 0.0, smoothstep(0.0, 0.2, t));
	fragColor = vec4(color, 1.0);

	// Convert pixel coord into uv centered at the origin
	float2 uv = fragCoord / iResolution.xy - scr_center;
	// Apply the zoom effect
	bgcol = 0.0;
	if (bBGTextureAvailable) bgcol.rgb = HyperZoom(uv);
	bgcol.a = 1.0;

	// Blend the trails with the current background
	float lightness = dot(0.333, color.rgb);
	color = lerp(bgcol.rgb, color, lightness);
	output.color = vec4(color, 1.0);

	// Fix the final bloom and mask it with the cockpit alpha
	output.bloom = float4(1.0 * float3(0.5, 0.5, 1) * bloom, bloom);
	output.bloom *= 1.0 - fgcol.a; // Hide the bloom mask wherever the foreground is solid
	return output;
}
