/*
 * Hyperspace streaks
 *
 * Created by Louis Sugy, 2019
 * You can use it under the terms of the MIT license
 * (free to use even in commercial projects, attribution required)
 *
 * Adapted by Leo Reyes for X-Wing Alliance, 2019.
 */

#include "ShaderToyDefs.h"

 // The Color Buffer (_shadertoyAuxBuf)
Texture2D colorTex : register(t0);
SamplerState colorSampler : register(s0);

static const vec3 blue_col = vec3(0.5, 0.7, 1);
static const float t2 = 2.0;
static const float t2_zoom = 1.5;
static const float t_overlap = 1.0;

inline float getTime() {
	//return mod(iTime, t2);
	return iTime - t2_zoom + t_overlap;
}

vec2 cart2polar(vec2 cart) {
	return vec2(atan2(cart.y, cart.x), length(cart));
}

// From https://www.shadertoy.com/view/4sc3z2
// and https://www.shadertoy.com/view/XsX3zB
#define MOD3 vec3(.1031,.11369,.13787)
vec3 hash33(vec3 p3)
{
	p3 = fract(p3 * MOD3);
	p3 += dot(p3, p3.yxz + 19.19);
	return -1.0 + 2.0 * fract(vec3((p3.x + p3.y)*p3.z, (p3.x + p3.z)*p3.y, (p3.y + p3.z)*p3.x));
}

float simplexNoise(vec3 p)
{
	const float K1 = 0.333333333;
	const float K2 = 0.166666667;

	vec3 i = floor(p + (p.x + p.y + p.z) * K1);
	vec3 d0 = p - (i - (i.x + i.y + i.z) * K2);

	vec3 e = step(0.0, d0 - d0.yzx);
	vec3 i1 = e * (1.0 - e.zxy);
	vec3 i2 = 1.0 - e.zxy * (1.0 - e);

	vec3 d1 = d0 - (i1 - 1.0 * K2);
	vec3 d2 = d0 - (i2 - 2.0 * K2);
	vec3 d3 = d0 - (1.0 - 3.0 * K2);

	vec4 h = max(0.6 - vec4(dot(d0, d0), dot(d1, d1), dot(d2, d2), dot(d3, d3)), 0.0);
	vec4 n = h * h * h * h * vec4(dot(d0, hash33(i)), dot(d1, hash33(i + i1)), dot(d2, hash33(i + i2)), dot(d3, hash33(i + 1.0)));

	return dot(31.316, n);
}

float jumpstep(float low, float high, float val)
{
	/*
	 The curve is plotted here:
	 https://www.iquilezles.org/apps/graphtoy/?f1(x)=atan(10%20*%20x%20-%205.0)%20/%20(2%20*%20atan(5.0))%20+%200.5&f2(x)=clamp(f1(x),%200,%201)&f3(x)=(10%20*%20x%20-%205.0)%20/%20(2%20*%20atan(5.0))%20+%200.5
	*/
	// This part of the curve looks like a smoothstep going from 0
	// to halfway up the curve
	float f1 = clamp(
		atan(8.0 * (val - low) / (high - low) - 5.0) / (2.0 * ATAN5) + 0.5,
		0.0, 1.0
	);
	// This is a linear curve
	float f2 = (8.0 * (val - low) / (high - low) - 5.0) / (2.0 * ATAN5) + 0.5;
	return max(f1, f2);
}

// Render the hyperspace streaks
vec3 pixelVal(vec2 coord, out float bloom)
{
	// Pixel to point (the center of the screen is at (0,0))
	vec2 resolution = iResolution * 4.0;
	vec2 uv = (2.0 * coord - resolution.xy) / resolution.x;
	vec2 ad = cart2polar(uv);
	// ad: polar coords
	// ad.x = angle
	// ad.y = radius

	// Loop forever
	float time = getTime();
	if (time < 0.0)
		return float3(0.0, 0.0, 0.0);
	// Uncomment this line to revert the effect
	//time = t2 - time;

	//time = 0.5 * t2; // DEBUG
	float t = time / t2; // normalized [0..1] time

	vec3 bg = 0.0;
	vec3 fg = 0.75 * vec3(0.082, 0.443, 0.7);
	vec3 col = mix(bg, fg, t);
	// whiteout:
	col = mix(col, 1.0, smoothstep(0.5, 0.9, t));

	//time = 1.25; // DEBUG
	float intensity = 1.0;
	// Smaller r's produce longer streaks
	float r = ad.y;
	r = r * 40.0 / (5.0 + 60.0 * jumpstep(0.0, t2, 0.5*pow(abs(time), 3.5)));

	// Lower values in the multiplier for ad.x yield thicker streaks
	float noiseVal = simplexNoise(vec3(60.0 * ad.x, r, 0.0));
	float noiseGain = 1.0 + 2.0 * smoothstep(0.5, 0.9, t);
	noiseVal *= noiseGain;
	// Let's remove a few streaks (but let's remove less streaks as time
	// progresses):
	float lo_t = clamp(mix(0.25, 0.0, t), 0.0, 1.0);
	noiseVal = smoothstep(lo_t, 1.0, noiseVal);

	intensity = mix(0.0, 10.0, t * 1.5);
	// Multiplying by ad.y darkens the center streaks a little bit
	noiseVal *= ad.y * intensity * noiseVal;
	float white_level = smoothstep(0.0, 1.0, noiseVal);
	white_level *= white_level;
	bloom = white_level;

	col += intensity * blue_col * noiseVal + white_level;

	///////////////////////////////////////
	// Add the white disk in the center
	///////////////////////////////////////
	float disk_size = 0.025, falloff, disk_col;
	float disk_intensity;
	//disk_size = clamp(1.7*(t - 0.25), 0.0, 0.5);
	disk_intensity = smoothstep(0.25, 0.65, t);

	// Don't render the disk when exiting hyperspace
	//falloff = 3.0; // 100 = short falloff, 3.0 = big fallof
	// Negative fallofs will make a black disk surrounded by a halo
	//disk_col = exp(-(ad.y - disk_size) * falloff);
	//col += disk_intensity * disk_col * vec3(0.913, 0.964, 0.980);

	return col;
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

	vec3 col = colorTex.SampleLevel(colorSampler, uv_scaled, 0).rgb;
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
	float4 pos3D    : SV_TARGET2;
	float4 normal   : SV_TARGET3;
	float4 ssaoMask : SV_TARGET4;
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
	factors[0] = 900.0; dist[0] = 5.00; dist_apply[0] = 1.0; fade[0] = 1.0;
	factors[1] = 60.0;  dist[1] = 5.00; dist_apply[1] = 1.0; fade[1] = 1.0;
	factors[2] = 10.0;  dist[2] = 5.00; dist_apply[2] = 1.0; fade[2] = 1.0;
	factors[3] = 5.0;   dist[3] = 5.00; dist_apply[3] = 1.0; fade[3] = 1.0;
	factors[4] = 3.0;   dist[4] = 0.25; dist_apply[4] = 0.8; fade[4] = 1.0;
	factors[5] = 2.0;   dist[5] = 0.15; dist_apply[5] = 0.2; fade[5] = 1.0;
	factors[6] = 1.5;   dist[6] = 0.00; dist_apply[6] = 0.0; fade[6] = 0.0;
	iters[0] = 32; iters[1] = 32; iters[2] = 32;
	iters[3] = 32; iters[4] = 32; iters[5] = 32;
	iters[6] = 1;
	d_scale[0] = 1.0; d_scale[1] = 1.0; d_scale[2] = 0.8;
	d_scale[3] = 0.8; d_scale[4] = 0.75; d_scale[5] = 0.4;
	d_scale[6] = 1.0;

	// The higher the uv multipler, the smaller the image
	//fragColor = texture(iChannel0, uv * 3.0 + scr_center);
	//return;

	//uv  = uv / mod(iTime, 5.0);
	//uv += scr_center;

	float t = min(1.0, clamp(iTime, 0.0, 100.0) / t2_zoom); // Normalize time in [0..1]
	t = 1.0 - t; // Time is reversed in this shader so that we can shrink the streaks
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

	output.pos3D = 0;
	output.normal = 0;
	output.ssaoMask = 1;
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

// Hyperspace streaks
PixelShaderOutput main(PixelShaderInput input)
{
	PixelShaderOutput output;
	vec4 fragColor = vec4(0.0, 0.0, 0.0, 1);
	vec2 fragCoord = input.uv * iResolution.xy;
	vec2 uv;
	vec3 streakcol = 0.0;
	float4 bgcol;
	float bloom = 0.0, white_level;

	output.pos3D = 0;
	output.normal = 0;
	output.ssaoMask = 1;
	output.bloom = 0;

	// Early exit: avoid rendering outside the original viewport edges
	if (input.uv.x < x0 || input.uv.x > x1 ||
		input.uv.y < y0 || input.uv.y > y1)
	{
		output.color = 0.0;
		return output;
	}

	// Render the streaks
	for (int i = -1; i <= 1; i++)
		for (int j = -1; j <= 1; j++) {
			streakcol += pixelVal(4.0 * fragCoord + vec2(i, j), white_level);
			bloom += white_level;
		}
	streakcol /= 9.0;
	//bloom /= 9.0;
	bloom = 0.0;
	output.bloom = float4(5.0 * float3(0.5, 0.5, 1) * bloom, bloom);

	if (bUseHyperZoom)
		streakcol *= float3(1.0, 0.0, 0.0);
	else
		streakcol *= float3(0.0, 0.0, 1.0);

	// Convert pixel coord into uv centered at the origin
	uv = fragCoord / iResolution.xy - scr_center;
	// Apply the zoom effect
	bgcol = 0.0;
	if (bUseHyperZoom) bgcol.rgb = HyperZoom(uv);
	bgcol.a = 1.0;

	// Output to screen
	fragColor = vec4(streakcol, 1.0);
	float lightness = dot(0.333, fragColor.rgb);
	// Mix the background color with the streaks
	fragColor = lerp(bgcol, fragColor, lightness);

	output.color = fragColor;
	return output;
}
