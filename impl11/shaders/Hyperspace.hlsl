/* 
 * Created by Louis Sugy, 2019
 * You can use it under the terms of the MIT license
 * (free to use even in commercial projects, attribution required)
 *
 * Adapted by Leo Reyes for X-Wing Alliance, 2019.
 */

#include "ShaderToyDefs.h"

static const float t0 = 0.0;
static const float t1 = 1.7;

// Change the anti-aliasing to 1 if it's too slow
#define AA 2
#define PI 3.14159265
#define ATAN5 1.37340076695

// The Color Buffer
Texture2D colorTex : register(t0);
SamplerState colorSampler : register(s0);

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

float linearstep(float low, float high, float val)
{
	return clamp((val - low) / (high - low), 0.0, 1.0);
}

float jumpstep(float low, float high, float val)
{
	if (2.0 * val < high + low) {
		//return clamp(atan(10.0 * (val - low) / (high - low) - 5.0) / (2.0 * ATAN5) + 0.5, 0.0, 1.0);
		return saturate( atan(10.0 * (val - low) / (high - low) - 5.0) / (2.0 * ATAN5) + 0.5 );
	}
	else
		return (10.0 * (val - low) / (high - low) - 5.0) / (2.0 * ATAN5) + 0.5;
}

vec3 pixelVal(vec2 coord)
{
	// Pixel to point (the center of the screen is (0,0)
	// The center of the screen in XWA is *not* the center of rotation:
	// the aiming reticle is a little over the center.
	vec2 resolution = iResolution * float(AA);
	vec2 uv = (2.0*coord - resolution.xy) / resolution.x;
	vec2 ad = cart2polar(uv);

	// Loop forever
	//float time = iTime % 8.0;
	float time = iTime % 2.2;
	//time = 2.2 - time; // This will invert the animation, good for fade-in

	vec3 bg = vec3(0, 0, 0.05);
	vec3 col = bg;

	// The effect starts
	if (time >= t0 && time < t1) {
		float blueTime = 1.0 - 1.0*smoothstep(t0, t1 + 0.5, time);
		float whiteTime = 1.0 - 0.6*smoothstep(t0 + 0.2, t1 + 0.5, time);
		float r = ad.y / (1.0 + 0.15 * linearstep(0.0, t1 - 1.0, time))
			*(40.0 / (3.0 + 20.0*jumpstep(t0, 6.0, 0.5*pow(abs(time), 1.5))));
		float noiseVal = simplexNoise(vec3(40.0*ad.x, r, 0.0));
		col = mix(col, vec3(0.5, 0.7, 1), smoothstep(t0, t1 - 1.0, time)
			*smoothstep(0.4*blueTime, blueTime, noiseVal));
		col = mix(col, vec3(1, 1, 1), smoothstep(t0, t1 - 1.0, time)
			*smoothstep(0.7 * whiteTime, whiteTime, noiseVal));

		// Dark at the center
		col = mix(col, bg, smoothstep(t0, t1 - 1.1, time) * (1.0 - ad.y));
	}

	// Fade to white
	//if (time > 4.2 && time < 4.5) {
	/*
	if (time > t1 - 0.3 && time < t1) {
		col = mix(col, vec3(0.9, 0.95, 1.0), smoothstep(t1 - 0.3, t1, time));
	}
	*/

	/*
	// Fade to black with stars (decelerating)
	else if (time > 4.5) {
		float r = ad.y / (1.0 + 2.0 / PI * atan(-0.042*(8.0 - time)) / cos((8.0 - time) / 2.2));
		float noiseVal = simplexNoise(vec3(60.0*ad.x, 50.0*r, 0.0));
		float whiteIntensity = smoothstep(0.7, 1.0, noiseVal);
		col = mix(col, vec3(1, 1, 1), whiteIntensity);
		col = mix(col, vec3(0.9, 0.95, 1.0), 1.0 - smoothstep(4.5, 5.0, time));
	}
	*/

	return col;
}

vec3 pixelVal_new(vec2 coord)
{
	// Pixel to point (the center of the screen is (0,0)
	vec2 resolution = iResolution * float(AA);
	vec2 uv = (2.0*coord - resolution.xy) / resolution.x;
	vec2 ad = cart2polar(uv);

	// Loop forever
	float tmax = 5.5;
	float time = iTime % tmax;
	//time = tmax - time;
	//time = 4.4;

	vec3 bg = vec3(0, 0, 0.05);
	vec3 col = bg;

	// Initial step, just roaming in space
	//if(time < 3.5) {

	// The effect starts
	if (time >= 2.8 && time < 4.5) {
		float blueTime = 1.0 - 1.0*smoothstep(2.8, 5.0, time);
		float whiteTime = 1.0 - 0.6*smoothstep(3.0, 5.0, time);
		float r = ad.y / (1.0 + 0.15*linearstep(0.0, 3.5, time))
			*(40.0 / (3.0 + 20.0*jumpstep(2.8, 6.0, 0.5*pow(abs(time), 1.5))));
		float noiseVal = simplexNoise(vec3(40.0*ad.x, r, 0.0));
		col = mix(col, vec3(0.5, 0.7, 1), smoothstep(2.8, 3.5, time)
			*smoothstep(0.4*blueTime, blueTime, noiseVal));
		col = mix(col, vec3(1, 1, 1), smoothstep(2.8, 3.5, time)
			*smoothstep(0.7 * whiteTime, whiteTime, noiseVal));

		// Dark at the center
		//col = mix(col, bg, smoothstep(2.8, 3.4, time) * (1.0 - ad.y));
		float radius = 1.0 - ad.y;
		//float disk = mix(0.0, 1.0, (time - 2.8) / (4.5 - 2.8));
		float disk = 0.12;
		radius += clamp((time - 2.8) * disk, 0.0, disk);
		radius = pow(radius, 5.0);
		col = mix(col, vec3(1, 1, 1), smoothstep(2.8, 3.4, time) * radius);
	}
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

PixelShaderOutput main1(PixelShaderInput input)
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

////////////////////////////////////////////////////////////////////////////////


// Based on theGiallo's https://www.shadertoy.com/view/MttSz2
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
	f = f * f*(3.0 - 2.0*f);	// Cosine interpolation approximation

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

PixelShaderOutput main(PixelShaderInput input)
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
	//col.rgb += mix(col.xyz, vec3(1,1,1), disk_col);
	col.rgb += vec3(disk_col, disk_col, disk_col);

	//vec4 fog_color = vec4(1.0,1.0,1.0,1.0);
	//float b = 2.0;
	//col = col*(1.0 - exp(-l*b)) + fog_color*exp(-l*b);

	fragColor = vec4(col.rgb, 1);
	output.color = fragColor;
	return output;
}

/* https://www.shadertoy.com/view/XsX3zB
 *
 * The MIT License
 * Copyright © 2013 Nikita Miropolskiy
 *
 * ( license has been changed from CCA-NC-SA 3.0 to MIT
 *
 *   but thanks for attributing your source code when deriving from this sample
 *   with a following link: https://www.shadertoy.com/view/XsX3zB )
 *
 * ~
 * ~ if you're looking for procedural noise implementation examples you might
 * ~ also want to look at the following shaders:
 * ~
 * ~ Noise Lab shader by candycat: https://www.shadertoy.com/view/4sc3z2
 * ~
 * ~ Noise shaders by iq:
 * ~     Value    Noise 2D, Derivatives: https://www.shadertoy.com/view/4dXBRH
 * ~     Gradient Noise 2D, Derivatives: https://www.shadertoy.com/view/XdXBRH
 * ~     Value    Noise 3D, Derivatives: https://www.shadertoy.com/view/XsXfRH
 * ~     Gradient Noise 3D, Derivatives: https://www.shadertoy.com/view/4dffRH
 * ~     Value    Noise 2D             : https://www.shadertoy.com/view/lsf3WH
 * ~     Value    Noise 3D             : https://www.shadertoy.com/view/4sfGzS
 * ~     Gradient Noise 2D             : https://www.shadertoy.com/view/XdXGW8
 * ~     Gradient Noise 3D             : https://www.shadertoy.com/view/Xsl3Dl
 * ~     Simplex  Noise 2D             : https://www.shadertoy.com/view/Msf3WH
 * ~     Voronoise: https://www.shadertoy.com/view/Xd23Dh
 * ~
 *
 */

/*
// discontinuous pseudorandom uniformly distributed in [-0.5, +0.5]^3
vec3 random3(vec3 c) {
	float j = 4096.0*sin(dot(c, vec3(17.0, 59.4, 15.0)));
	vec3 r;
	r.z = fract(512.0*j);
	j *= .125;
	r.x = fract(512.0*j);
	j *= .125;
	r.y = fract(512.0*j);
	return r - 0.5;
}

// skew constants for 3d simplex functions 
const float F3 = 0.3333333;
const float G3 = 0.1666667;

// 3d simplex noise 
float simplex3d(vec3 p) {
	// 1. find current tetrahedron T and it's four vertices 
	// s, s+i1, s+i2, s+1.0 - absolute skewed (integer) coordinates of T vertices 
	// x, x1, x2, x3 - unskewed coordinates of p relative to each of T vertices

	// calculate s and x 
	vec3 s = floor(p + dot(p, vec3(F3)));
	vec3 x = p - s + dot(s, vec3(G3));

	// calculate i1 and i2 
	vec3 e = step(vec3(0.0), x - x.yzx);
	vec3 i1 = e * (1.0 - e.zxy);
	vec3 i2 = 1.0 - e.zxy*(1.0 - e);

	// x1, x2, x3 
	vec3 x1 = x - i1 + G3;
	vec3 x2 = x - i2 + 2.0*G3;
	vec3 x3 = x - 1.0 + 3.0*G3;

	// 2. find four surflets and store them in d 
	vec4 w, d;

	// calculate surflet weights 
	w.x = dot(x, x);
	w.y = dot(x1, x1);
	w.z = dot(x2, x2);
	w.w = dot(x3, x3);

	// w fades from 0.6 at the center of the surflet to 0.0 at the margin 
	w = max(0.6 - w, 0.0);

	// calculate surflet components
	d.x = dot(random3(s), x);
	d.y = dot(random3(s + i1), x1);
	d.z = dot(random3(s + i2), x2);
	d.w = dot(random3(s + 1.0), x3);

	// multiply d by w^4 
	w *= w;
	w *= w;
	d *= w;

	// 3. return the sum of the four surflets 
	return dot(d, vec4(52.0));
}

// const matrices for 3d rotation 
const mat3 rot1 = mat3(-0.37, 0.36, 0.85, -0.14, -0.93, 0.34, 0.92, 0.01, 0.4);
const mat3 rot2 = mat3(-0.55, -0.39, 0.74, 0.33, -0.91, -0.24, 0.77, 0.12, 0.63);
const mat3 rot3 = mat3(-0.71, 0.52, -0.47, -0.08, -0.72, -0.68, -0.7, -0.45, 0.56);

// directional artifacts can be reduced by rotating each octave
float simplex3d_fractal(vec3 m) {
	return   0.5333333*simplex3d(m*rot1)
		+ 0.2666667*simplex3d(2.0*m*rot2)
		+ 0.1333333*simplex3d(4.0*m*rot3)
		+ 0.0666667*simplex3d(8.0*m);
}

void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
	vec2 p = fragCoord.xy / iResolution.x;
	vec3 p3 = vec3(p, iTime*0.025);

	float value;

	value = simplex3d_fractal(p3*8.0 + 8.0);

	value = 0.5 + 1.5*value;

	fragColor = vec4(
		vec3(value),
		1.0);
	return;
}
*/

/*
From: https://www.shadertoy.com/view/4lB3zz

const int firstOctave = 3;
const int octaves = 8;
const float persistence = 0.6;

//Not able to use bit operator like <<, so use alternative noise function from YoYo
//
//https://www.shadertoy.com/view/Mls3RS
//
//And it is a better realization I think
float noise(int x,int y)
{
	float fx = float(x);
	float fy = float(y);

	return 2.0 * fract(sin(dot(vec2(fx, fy) ,vec2(12.9898,78.233))) * 43758.5453) - 1.0;
}

float smoothNoise(int x,int y)
{
	return noise(x,y)/4.0+(noise(x+1,y)+noise(x-1,y)+noise(x,y+1)+noise(x,y-1))/8.0+(noise(x+1,y+1)+noise(x+1,y-1)+noise(x-1,y+1)+noise(x-1,y-1))/16.0;
}

float COSInterpolation(float x,float y,float n)
{
	float r = n*3.1415926;
	float f = (1.0-cos(r))*0.5;
	return x*(1.0-f)+y*f;

}

float InterpolationNoise(float x, float y)
{
	int ix = int(x);
	int iy = int(y);
	float fracx = x-float(int(x));
	float fracy = y-float(int(y));

	float v1 = smoothNoise(ix,iy);
	float v2 = smoothNoise(ix+1,iy);
	float v3 = smoothNoise(ix,iy+1);
	float v4 = smoothNoise(ix+1,iy+1);

	float i1 = COSInterpolation(v1,v2,fracx);
	float i2 = COSInterpolation(v3,v4,fracx);

	return COSInterpolation(i1,i2,fracy);

}

float PerlinNoise2D(float x,float y)
{
	float sum = 0.0;
	float frequency = 1.0;
	float amplitude = 1.0;
	for(int i=firstOctave;i<octaves + firstOctave;i++)
	{
		frequency = pow(2.0,float(i));
		amplitude = pow(persistence,float(i));
		sum = sum + InterpolationNoise(x*frequency,y*frequency)*amplitude;
	}

	return sum;
}

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{

	vec2 uv = fragCoord.xy / iResolution.x;
	float x = uv.x + iTime * 0.025;
	float y = uv.y + iTime * 0.25;
	//fragColor = vec4(noise(x,y),noise(x,y),noise(x,y),1);
	float noise = 0.3 + 5.0*PerlinNoise2D(x * 4.0, y * 2.0);
	fragColor = vec4(noise,noise,noise,1.0);
}
*/