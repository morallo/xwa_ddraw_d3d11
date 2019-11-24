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

#define PI 3.14159265
#define ATAN5 1.37340076695

// The Color Buffer
Texture2D colorTex : register(t0);
SamplerState colorSampler : register(s0);

static const vec3 blue_col = vec3(0.5, 0.7, 1);
static const float t2 = 2.0;
//static const float t3 = t2 + 0.3;

inline float getTime() {
	return mod(iTime, t2);
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

/*
float jumpstep(float low, float high, float val)
{
	if (2.0 * val < high + low)
		return saturate(atan(10.0 * (val - low) / (high - low) - 5.0) / (2.0 * ATAN5) + 0.5);
	else
		return (10.0 * (val - low) / (high - low) - 5.0) / (2.0 * ATAN5) + 0.5;
}
*/

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

/*
// Render the hyperspace streaks
vec3 pixelVal(vec2 coord)
{
	// Pixel to point (the center of the screen is at (0,0)
	vec2 resolution = iResolution * 4.0;
	vec2 uv = (2.0 * coord - resolution.xy) / resolution.x;
	vec2 ad = cart2polar(uv);
	// ad: polar coords
	// ad.x = angle
	// ad.y = radius

	// Loop forever
	float time = getTime();
	// Uncomment this line to revert the effect
	//time = t3 - time;

	vec3 bg = 0.0;
	vec3 fg = 0.75 * vec3(0.082, 0.443, 0.7);
	vec3 col = mix(bg, fg, time / t2);

	//time = 1.25; // DEBUG
	float intensity = 1.0;
	// Smaller r's produce longer streaks
	float r = ad.y;
	
	r = r / (1.0 + 0.15 * linearstep(0.0, t2 / 2.0, time)) *
				(40.0 / (3.0 + 20.0 *
							jumpstep(0.0, t2, 0.5 * pow(abs(time), 1.5))
						)
				);

	// Lower values in the multiplier for ad.x yield thicker streaks
	float noiseVal = simplexNoise(vec3(60.0 * ad.x, r, 0.0));
	// This line removes a few streaks, but keeps streaks thin:
	noiseVal = smoothstep(0.0, 1.0, noiseVal);

	intensity = mix(0.5, 10.0, time / t2);
	noiseVal *= intensity * noiseVal;
	float white_level = smoothstep(0.0, 1.0, noiseVal);
	white_level *= white_level;

	col += intensity * blue_col * noiseVal + white_level;

	// Add the white disk in the center
	float disk_size, falloff, disk_col;
	float t = time / t2 - t2 * 0.166;
	t = clamp(t, 0.0, 1.0);
	disk_size = mix(0.0, 1.0, t);
	//disk_size = pow(disk_size, 2.0);
	disk_size = 1.5 * pow(disk_size, 2.5);
	//if (disk_size < 0.01) disk_size = 0.0;
	disk_size = clamp(disk_size, 0.0, 1.0);

	falloff = 3.0;
	// Negative fallofs will make a black disk surrounded by a halo
	disk_col = exp(-(ad.y - disk_size) * falloff);
	col += disk_size * disk_col * vec3(0.913, 0.964, 0.980);
	return col;
}
*/

// Render the hyperspace streaks
vec3 pixelVal(vec2 coord)
{
	// Pixel to point (the center of the screen is at (0,0))
	vec2 resolution = iResolution * 4.0;
	vec2 uv = (2.0 * coord - resolution.xy) / resolution.x;
	vec2 ad = cart2polar(uv);
	// ad: polar coords
	// ad.x = angle
	// ad.y = radius

	// Loop forever
	float time = mod(iTime, t2);
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

	col += intensity * blue_col * noiseVal + white_level;

	///////////////////////////////////////
	// Add the white disk in the center
	///////////////////////////////////////
	float disk_size = 0.025, falloff, disk_col;
	float disk_intensity;
	//disk_size = clamp(1.7*(t - 0.25), 0.0, 0.5);
	disk_intensity = smoothstep(0.25, 0.65, t);

	//disk_size = jumpstep(0.0, t2 - 0.3, 0.5*pow(time, 3.5));

	//falloff = mix(250.0, 3.0, time/t2); // 100 = short fallof, 3.0 = big falloff
	falloff = 3.0;
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
	float4 color : SV_TARGET0;
};

// Hyperspace streaks
PixelShaderOutput main(PixelShaderInput input)
{
	PixelShaderOutput output;
	vec4 fragColor = vec4(0.0, 0.0, 0.0, 1);
	vec2 fragCoord = input.uv * iResolution.xy;
	vec3 avgcol = 0.0;
	float time = getTime();
	float4 col = colorTex.Sample(colorSampler, input.uv);

	// Early exit: avoid rendering outside the original viewport edges
	if (input.uv.x < x0 || input.uv.x > x1 ||
		input.uv.y < y0 || input.uv.y > y1)
	{
		output.color = 0.0;
		return output;
	}

	// Fade the background color to black
	col = lerp(col, 0, 1.1 * fract(time / t2));

	for (int i = -1; i <= 1; i++)
		for (int j = -1; j <= 1; j++)
			avgcol += pixelVal(4.0 * fragCoord + vec2(i, j));
	avgcol /= 9.0;
	
	// Output to screen
	fragColor = vec4(avgcol, 1.0);
	float lightness = dot(0.333, fragColor);
	// Mix the background color with the streaks
	fragColor = lerp(col, fragColor, lightness);

	// DEBUG
	//fragColor = col;
	// DEBUG
	output.color = fragColor;
	return output;
}

/*
PixelShaderOutput main(PixelShaderInput input)
{
	PixelShaderOutput output;
	vec4 fragColor = vec4(0.0, 0.0, 0.0, 1);
	vec2 fragCoord = input.uv * iResolution.xy;
	vec3 avgcol = 0;
	float4 col = colorTex.Sample(colorSampler, input.uv);
	// Fade the background color
	float time = iTime % 2.2;
	//time = 4.4;
	col = lerp(col, 0, time / (t1 * 0.5));

	for (int i = 0; i < AA; i++)
		for (int j = 0; j < AA; j++)
			avgcol += pixelVal(float(AA)*fragCoord + vec2(i, j));

	avgcol /= float(AA*AA);

	// Output to screen
	fragColor = vec4(avgcol, 1.0);
	//fragColor = fragColor / (1 + fragColor);
	float L = dot(0.333, fragColor);
	// Mix the background color with the streaks
	fragColor = lerp(col, fragColor, L);
	//fragColor = col + fragColor;
	output.color = fragColor;
	return output;
}
*/

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

/*
 * Hyperspace Tunnel
 * Based on theGiallo's shader: https://www.shadertoy.com/view/MttSz2
 */

#define TAU   6.28318
static const float3 blue_color = float3(0.15, 0.35, 0.9);
static const float period = 3.7;
static const float rotation_speed = 2.3;
static const float speed = 2.0;
// The focal_depth controls how "deep" the tunnel looks. Lower values
// provide more depth.
static const float focal_depth = 0.15;


// Perlin noise from Dave_Hoskins' https://www.shadertoy.com/view/4dlGW2
//----------------------------------------------------------------------------------------
float Hash(in vec2 p, in float scale)
{
	// This is tiling part, adjusts with the scale...
	p = p % scale;
	return fract(sin(dot(p, vec2(27.16898, 38.90563))) * 5151.5473453);
}

//----------------------------------------------------------------------------------------
float Noise(in vec2 p, in float scale)
{
	vec2 f;
	p *= scale;
	f = fract(p);		// Separate integer from fractional

	p = floor(p);
	f = f * f * (3.0 - 2.0 * f);	 // Cosine interpolation approximation

	float res = mix(mix(Hash(p, scale),
		Hash(p + vec2(1.0, 0.0), scale), f.x),
		mix(Hash(p + vec2(0.0, 1.0), scale),
			Hash(p + vec2(1.0, 1.0), scale), f.x), f.y);
	return res;
}

//----------------------------------------------------------------------------------------
float fBm(in vec2 p)
{
	//p += vec2(sin(iTime * .7), cos(iTime * .45))*(.1) + iMouse.xy*.1/iResolution.xy;
	float f = 0.0;
	// Change starting scale to any integer value...
	float scale = 10.;
	p = mod(p, scale);
	float amp = 0.6;

	for (int i = 0; i < 5; i++)
	{
		f += Noise(p, scale) * amp;
		amp *= .5;
		// Scale must be multiplied by an integer value...
		scale *= 2.0;
	}
	// Clamp it just in case....
	return min(f, 1.0);
}

PixelShaderOutput main_tunnel(PixelShaderInput input)
{
	PixelShaderOutput output;
	vec4 fragColor = vec4(0.0, 0.0, 0.0, 1);
	vec2 fragCoord = input.uv * iResolution.xy;

	vec2 q = fragCoord.xy / iResolution.xy;
	vec2 qc = 2.0 * q - 1.0;
	vec2 p = (2.0 * fragCoord.xy - iResolution.xy) / min(iResolution.y, iResolution.x);
	vec4 col = vec4(0, 0, 0, 1);

	vec2 cp;
	vec2 dp = p;
	float dp_len = length(dp);
	cp.y = focal_depth / dp_len + iTime * speed;
	// Remove the seam by reflecting the u coordinate around 0.5:
	float a = atan2(dp.y, dp.x) + PI; // a is now in [0..2*PI]
	a -= iTime * rotation_speed;
	float x = frac(a / TAU);
	if (x >= 0.5) x = 1.0 - x;
	cp.x = x * period; // Original period: 4

	float val = fBm(0.75 * cp);
	col.rgb = val * blue_color;

	// Add white spots
	vec3 white = 0.3 * smoothstep(0.65, 1.0, val);
	col.rgb += white;

	// Add the white disk at the center
	float disk_size = 0.125;
	float disk_col = exp(-(dp_len - disk_size) * 3.0);
	col.rgb += vec3(disk_col, disk_col, disk_col);

	fragColor = vec4(col.rgb, 1);
	output.color = fragColor;
	return output;
}
