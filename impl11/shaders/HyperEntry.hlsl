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

// Color buffer (foreground/cockpit)
Texture2D fgColorTex : register(t0);
SamplerState fgColorSampler : register(s0);

// Color buffer (background)
Texture2D bgColorTex : register(t1);
SamplerState bgColorSampler : register(s1);

static const vec3 blue_col = vec3(0.5, 0.7, 1);
static const float t2 = 2.0;
//static const float t3 = t2 + 0.3;

inline float getTime() {
	//return mod(iTime, t2);
	return min(iTime, t2);
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
	bloom = white_level;

	col += intensity * blue_col * noiseVal + white_level;

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

// Hyperspace streaks
PixelShaderOutput main(PixelShaderInput input)
{
	PixelShaderOutput output;
	vec4 fragColor = vec4(0.0, 0.0, 0.0, 1);
	vec2 fragCoord = input.uv * iResolution.xy;
	vec3 streakcol = 0.0;
	float time = getTime();
	float4 fgcol = fgColorTex.Sample(fgColorSampler, input.uv);
	float4 bgcol = bgColorTex.Sample(bgColorSampler, input.uv);
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

	// Fade the background color
	//bgcol = lerp(bgcol, 0.75 * bgcol, fract(time / t2));

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
