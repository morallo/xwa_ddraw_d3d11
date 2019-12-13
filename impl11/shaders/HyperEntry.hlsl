/*
 * Hyperspace trails. MIT License.
 *
 * You can use it under the terms of the MIT license
 * (free to use even in commercial projects, attribution required)
 *
 * Adapted from 	https://www.shadertoy.com/view/MlKBWw by Leo Reyes
 * Shadertoy implementation: https://www.shadertoy.com/view/3l3Gz8
 *
 * Lens flare from: https://www.shadertoy.com/view/XdfXRX
 * and: https://www.shadertoy.com/view/4sX3Rs
 * (c) Leo Reyes, 2019.
 */

#include "ShaderToyDefs.h"

// Color buffer (foreground/cockpit)
Texture2D fgColorTex : register(t0);
SamplerState fgColorSampler : register(s0);

// Color buffer (background)
Texture2D bgColorTex : register(t1);
SamplerState bgColorSampler : register(s1);

//static const float t2 = 2.0;

// The way this shader works is by looking at the screen as if it were a disk and then
// this disk is split into a number of slices centered at the origin. Each slice renders
// a single trail. So this setting controls the overall density of the effect:
#define NUM_SLICES 125.0

// Each trail is rendered within its slice; but to avoid generating regular patterns, we
// randomly offset the trail from the center of the slice by this amount:
#define MAX_SLICE_OFFSET 0.4

// This is the length of the effect in seconds:
#define T_MAX 2.0
// T_JUMP is in normalized [0..1] time: this is the time when the 
// trails zoom out of view because we've jumped into hyperspace:
#define T_JUMP 0.75
// This is the speed during the final jump:
#define jump_speed 15.0

#define FLARE 1

// I've noticed that the effect tends to have a bluish tint. In this 
// shader, the blue color is towards the start of the trail, and the 
// white color towards the end:
static const vec3 blue_col = vec3(0.3, 0.3, 0.5);
//static const vec3 blue_col = vec3(0.5, 0.7, 1);
//const vec3 white_col = vec3(0.95, 0.95, 1.0);
static const vec3 white_col = vec3(0.85, 0.85, 0.9);
static const vec3 flare_col = vec3(0.9, 0.9, 1.4);

/*
inline float getTime() {
	return min(iTime, t2);
}

vec2 cart2polar(vec2 cart) {
	return vec2(atan2(cart.y, cart.x), length(cart));
}
*/

/*
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
	// The curve is plotted here:
	// https://www.iquilezles.org/apps/graphtoy/?f1(x)=atan(10%20*%20x%20-%205.0)%20/%20(2%20*%20atan(5.0))%20+%200.5&f2(x)=clamp(f1(x),%200,%201)&f3(x)=(10%20*%20x%20-%205.0)%20/%20(2%20*%20atan(5.0))%20+%200.5
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
	bloom = 0.0;
	// ad: polar coords
	// ad.x = angle
	// ad.y = radius

	// Loop forever
	float time = getTime();
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
	bloom = t * white_level; // Increase the bloom with time

	col += intensity * blue_col * noiseVal + white_level;
	//bloom = noiseVal; // +white_level;

	///////////////////////////////////////
	// Add the white disk in the center
	///////////////////////////////////////
	float disk_size = 0.025, falloff, disk_col;
	float disk_intensity;
	//disk_size = clamp(1.7*(t - 0.25), 0.0, 0.5);
	disk_intensity = smoothstep(0.25, 0.65, t);

	falloff = 3.0; // 100 = short falloff, 3.0 = big fallof
	// Negative fallofs will make a black disk surrounded by a halo
	disk_col = exp(-(ad.y - disk_size) * falloff);
	col += disk_intensity * disk_col * vec3(0.913, 0.964, 0.980);

	return col;
}
*/

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

/*
// Hyperspace streaks
PixelShaderOutput main_old(PixelShaderInput input)
{
	PixelShaderOutput output;
	vec4 fragColor = vec4(0.0, 0.0, 0.0, 1);
	vec2 fragCoord = input.uv * iResolution.xy;
	vec3 streakcol = 0.0;
	float time = getTime();
	float4 fgcol = fgColorTex.Sample(fgColorSampler, input.uv);
	float4 bgcol = bgColorTex.Sample(bgColorSampler, input.uv);
	float bloom = 0.0, white_level;
	output.bloom = 0;

	// Early exit: avoid rendering outside the original viewport edges
	if (input.uv.x < x0 || input.uv.x > x1 ||
		input.uv.y < y0 || input.uv.y > y1)
	{
		output.color = 0.0;
		return output;
	}

	// Fade the background color
	//bgcol = lerp(bgcol, 0.75 * bgcol, fract(time / t2));
	bloom = 0.0;
	for (int i = -1; i <= 1; i++)
		for (int j = -1; j <= 1; j++) {
			streakcol += pixelVal(4.0 * fragCoord + vec2(i, j), white_level);
			bloom += white_level;
		}
	streakcol /= 9.0;
	bloom /= 9.0;
	output.bloom = float4(5.0 * float3(0.5, 0.5, 1) * bloom, bloom);
	output.bloom *= 1.0 - fgcol.a; // Hide the bloom mask wherever the foreground is solid
	
	// Output to screen
	fragColor = vec4(streakcol, 1.0);
	float lightness = dot(0.333, fragColor.rgb);
	// Mix the background color with the streaks
	fragColor = lerp(bgcol, fragColor, lightness);

	// DEBUG
	//fragColor = bgcol;
	// DEBUG

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

vec3 lensflare(vec3 uv, vec3 pos, float flare_size, float ang_offset)
{
	float z = uv.z / length(uv.xy);
	vec2 main = uv.xy - pos.xy;
	float dist = length(main);
	float num_points = 2.71;
	float disk_size = 0.2;
	float inv_size = 1.0 / flare_size;
	float ang = atan2(main.y, main.x) + ang_offset;
	float fade = (z < 0.0) ? -z : 1.0;

	float f0 = 1.0 / (dist * inv_size + 1.0);
	f0 = f0 + f0 * (0.1 * sin((sin(ang*2.0 + pos.x)*4.0 - cos(ang*3.0 + pos.y)) * num_points) + disk_size);
	//if (z < 0.0) // Remove the flare on the back
	//	return clamp(mix(f0, 0.0, 0.75 * fade), 0.0, 1.0);
	//else
		return f0;
}

vec3 cc(vec3 color, float factor, float factor2) // color modifier
{
	float w = color.x + color.y + color.z;
	return mix(color, w * factor, w * factor2);
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
	float t = mod(iTime, T_MAX) / T_MAX;
	vec2 p = (2.0 * fragCoord.xy - iResolution.xy) / min(iResolution.x, iResolution.y);
	p += vec2(0, y_center); // In XWA the aiming HUD is not at the screen's center

	//float ta = TAU * mod(iTime, 8.0) / 8.0;
	//ta = 12.0 * 0.01745;

	
	float ay = 0.0, ax = 0.0, az = 0.0;
	//ay = t * TAU * 0.4;
	mat3 mY = mat3(
		 cos(ay), 0.0, sin(ay),
		 0.0,     1.0,     0.0,
		-sin(ay), 0.0, cos(ay)
	);

	mat3 mX = mat3(
		1.0,      0.0,     0.0,
		0.0,  cos(ax), sin(ax),
		0.0, -sin(ax), cos(ax)
	);

	/*
	mat3 mZ = mat3(
		cos(az), sin(az), 0.0,
	   -sin(az), cos(az), 0.0,
			0.0,     0.0, 1.0
	);
	mat3 m = mX * mY;
	*/

	float p_len = length(p);
	vec3 v = vec3(p, -1.0);
	v = mul(viewMat, vec4(v, 0.0)).xyz;

	float trail_start, trail_end;
	// Fade all the trails into view from black to a little above full-white:
	float fade = clamp(mix(0.1, 1.1, t * 2.0), 0.0, 2.0);
	float bloom = 0.0;

	// Each slice renders a single trail; but we can render multiple layers of
	// slices to add more density and randomness to the effect:
	for (float i = 0.0; i < 40.0; i++)
	{
		vec3 trail_color = 0.0;
		float angle = atan2(v.y, v.x) / 3.141592 / 2.0 + 0.13 * i;

		float slice = floor(angle * NUM_SLICES);
		float slice_fract = fract(angle * NUM_SLICES);

		// Don't center the trail in the slice: wiggle it a little bit:
		float slice_offset = MAX_SLICE_OFFSET *
			rand(vec2(slice, 4.0 + i * 25.0)) - (MAX_SLICE_OFFSET / 2.0);
		// This is the speed of the current slice
		float speed = 5.0 * rand(vec2(slice, 1.0 + i * 10.0)) + i * 0.01;

		// Without dist, all trails get stuck to the walls of the
		// tunnel.
		float dist = rand(vec2(slice, 1.0 + i * 10.0)) * (2.0 + i);
		float z = dist * v.z / length(v.xy);

		trail_start = 5.0 + 2.0 * rand(vec2(slice, 0.0 + i * 10.0));
		// Make half the trails appear behind the camera, so that
		// there's something behind the camera if we look at it:
		if (i > 20.0)
			trail_start = 10.0 * rand(vec2(slice, 0.0 + i * 10.0)) - 5.0;
		// Accelerate the trail_start:
		trail_start -= mix(0.0, jump_speed, smoothstep(T_JUMP, 1.0, t));
		trail_end = trail_start - t * speed; // + 0.01;

		float val = 0.0;
		//float trail_x = smoothstep(trail_start, trail_end, p_len);
		float trail_x = smoothstep(trail_start, trail_end, z);
		trail_color = mix(blue_col, white_col, trail_x);

		// This line computes the distance from the current pixel, in "slice-coordinates"
		// to the ideal trail centered at the slice center. The last argument makes the lines
		// a bit thicker when they reach the edges as time progresses.
		float h = sdLine(
			vec2(slice_fract + slice_offset, z),
			vec2(0.5, trail_start),
			vec2(0.5, trail_end),
			mix(0.0, 0.015, t * z));

		//h = smoothstep(0.075, 0.01, abs(h) - 0.05);
		// This threshold adds a "glow" to the line. This glow grows with
		// time:
		//float threshold = mix(0.04, 0.175, smoothstep(0.0, 0.5, t));
		float threshold = 0.09;
		h = (h < 0.01) ? 1.0 : 0.85 * smoothstep(threshold, 0.0, abs(h));

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

	if (bDisneyStyle) {
		// Add the disk at the center to transition into the hyperspace
		// tunnel
		float flare_size = mix(0.0, 0.1, smoothstep(0.35, T_JUMP + 0.2, t));
		flare_size += mix(0.0, 20.0, smoothstep(T_JUMP + 0.05, 1.0, t));
		vec3 flare = flare_col *
			lensflare(v, 0.0, flare_size, 0.1 * flare_size * t);
		color += cc(flare, 0.5, 0.1);
		// Whiteout
		color += mix(0.0, 1.0, smoothstep(T_JUMP + 0.1, 1.0, t));
	}
	else {
		// Whiteout
		color += mix(0.0, 1.0, smoothstep(T_JUMP - 0.2, 1.0, t));
	}

	// Blend the trails with the current background
	float lightness = dot(0.333, color.rgb);
	color = lerp(bgcol.rgb, color, lightness);
	output.color = vec4(color, 1.0);
	// Fix the final bloom and mask it with the cockpit alpha
	output.bloom = float4(1.0 * float3(0.5, 0.5, 1) * bloom, bloom);
	output.bloom *= 1.0 - fgcol.a; // Hide the bloom mask wherever the foreground is solid
	return output;
}