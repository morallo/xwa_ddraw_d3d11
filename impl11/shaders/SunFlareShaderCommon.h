/*
 * Sun/Star Shader.
 *
 * You can use this shader under the terms of the MIT license, see LICENSE.TXT
 * (free to use even in commercial projects, attribution required)
 *
 * from: https://www.shadertoy.com/view/XdfXRX
 * musk's lense flare, modified by icecool.
 * See the original at: https://www.shadertoy.com/view/4sX3Rs
 * Also from: https://www.shadertoy.com/view/Xlc3D2
 *
 * (c) Leo Reyes, 2020.
 */

#include "shading_system.h"
#include "ShaderToyDefs.h"
#include "ShadertoyCBuffer.h"
#include "metric_common.h"

// The "background" texture. This is the current render:
// cockpit, objects, lasers, everything. It's called "background"
// here because we're rendering the sun flare on top of it.
#ifdef INSTANCED_RENDERING
Texture2DArray bgTex : register(t0);
#else
Texture2D      bgTex : register(t0);
#endif

#ifdef GENMIPMAPS_DEBUG
// DEBUG section. This will enable mip maps in this shader
// Use the following definition when mip maps are enabled:
SamplerState bgSampler : register(s0) =
	sampler_state {
		Filter = MIN_MAG_MIP_LINEAR;
	};
#else
SamplerState bgSampler : register(s0);
#endif


// The depth buffer: we'll use this as a mask since the sun should be at infinity
Texture2D    depthTex     : register(t1);
SamplerState depthSampler : register(s1);

// The stellar backdrop texture: this is _only_ the stars and other backdrops.
// We want to know if a sun is visible or not
#ifdef INSTANCED_RENDERING
Texture2DArray stellarBckTex : register(t2);
#else
Texture2D      stellarBckTex : register(t2);
#endif

#define INFINITY_Z 30000.0

// DEBUG
#define cursor_radius 0.04
#define thickness 0.02 //0.007
#define scale 2.0
// DEBUG

/*
	// saturation test:

	vec3 col = vec3(0.0, 0.0, 1.0);
	float g = dot(vec3(0.333), col);
	vec3 gray = vec3(g);
	// saturation test
	float sat = 2.0;
	vec3 final_col = gray + sat * (col - gray);

 */

struct PixelShaderInput
{
	float4 pos    : SV_POSITION;
	float4 color  : COLOR0;
	float2 uv     : TEXCOORD0;
    float4 pos3D  : COLOR1;
#ifdef INSTANCED_RENDERING
    uint   viewId : SV_RenderTargetArrayIndex;
#endif
};

struct PixelShaderOutput
{
	float4 color  : SV_TARGET0;
};

/*
float sdLine(in vec2 p, in vec2 a, in vec2 b)
{
	vec2 pa = p - a, ba = b - a;
	float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
	return length(pa - ba * h);
}
*/

/*
float noise(float t)
{
	return texture(iChannel0, vec2(t, 0.0) / iChannelResolution[0].xy).x;
}

float noise(vec2 t)
{
	return texture(iChannel0, (t + vec2(iTime)) / iChannelResolution[0].xy).x;
}
*/

/*
// Simple star lens flare
float lensflare(vec2 uv, vec2 pos, float flare_size, float ang_offset)
{
	vec2 main = uv - pos;
	float dist = length(main);
	float num_points = 2.71;
	float disk_size = 0.2;
	float inv_size = 1.0 / flare_size;
	float ang = atan2(main.y, main.x) + ang_offset;

	float f0 = 1.0 / (dist * inv_size + 1.0);
	f0 = f0 + f0 * (0.1 * sin((sin(ang*4.0 + pos.x)*4.0 - cos(ang*3.0 + pos.y)) * num_points) + disk_size);
	return f0;
}
*/

float rnd(float w)
{
	float f = fract(sin(w)*1000.);
	return f;
}

float regShape(vec2 p, int N)
{
	float f;
	float a = atan2(p.x, p.y) + 0.2;
	float b = 6.28319 / float(N);
	f = smoothstep(0.5, 0.51, cos(floor(0.5 + a / b)*b - a) * length(p.xy));
	return f;
}

float anamorphicFlare(vec2 U)
{
	vec2 A = vec2(0.0, 1.0);
	U = mul(
		float2x2(32, 2, 1, 2),
		abs(mul(float2x2(A, -A.y, A.x), U))
	);
	return 0.075 / max(U.x, U.y);
}

/*
// The following uses a squashed Gaussian to do the anamorphic flare. It works; but
// it feels too "thick" so I didn't quite like it. Maybe I had to dim it a little more.
float anamorphicFlare(vec2 pos)
{
	pos.x *= 0.02;
	float L = dot(pos, pos);
	return 0.1 * exp(-L * 2500.0);
}
*/

// Hexagonal rays for the sun
// from https://www.shadertoy.com/view/ltfGDs
// 2dFoldings, inspired by Gaz/Knighty  see: https://www.shadertoy.com/view/4tX3DS
vec2 foldHex(in vec2 p)
{
	p.xy = abs(p.xy);
	const vec2 pl1 = vec2(-0.5, 0.8657);
	const vec2 pl2 = vec2(-0.8657, 0.5);
	p -= pl1 * 2.*min(0., dot(p, pl1));
	p -= pl2 * 2.*min(0., dot(p, pl2));
	return p;
}

// Generates thick rays at regular intervals given by num_points
float thickRays(vec2 uv, vec2 pos, float flare_size)
{
	vec2 main = uv - pos;
	float dist = length(main);
	float num_points = 6.0;
	float inv_size = 1.0 / flare_size;
	float ang = atan2(main.y, main.x) - 0.25;

	float f0 = 1.0 / (dist * inv_size + 1.0);

	//float ofs = 0.5 * iTime;
	float ofs = 0.0;
	float f1 = sin(ang * num_points);
	f0 = f0 * f1;
	//f0 = exp(-f0 * 100.0);
	return max(0.0, f0);
}

float multiRays(vec2 uv, vec2 pos, float freq1, float freq2, float speed)
{
	vec2 main = uv - pos;
	vec2 uvd = uv * (length(uv));

	float ang = atan2(main.y, main.x);
	float dist = length(main); dist = pow(dist, .1);
	//float n = noise(vec2((ang-iTime/9.0)*16.0,dist*32.0));

	float ofs = speed * iTime;
	float f0 = 0.0;
	f0 = 1.0 / (length(uv - pos) * 16.0 + 1.0);
	//f0 = f0 * (sin(ang * 64.0) * 1.9 /* + dist * 0.1+0.8 */);

	f0 = f0 * (
		sin(
			( 
		      sin(ang*freq1 + pos.x + ofs) * 4.0 +
			  cos(ang*freq2 + pos.y + ofs)
			) * (2.5 + sin(ofs))
		)
	);

	return max(f0*f0, 0.0);
}

// Full lens flare (the star spikes have been commented out)
vec3 lensflare(vec2 coord, vec2 flare_pos, int sun_index)
{
	//vec2 main = coord - flare_pos;
	vec2 uvd  = coord * length(coord);

	//float ang = atan2(main.y, main.x);
	//float dist = length(main); dist = pow(dist, .1);
	//float n = noise(vec2((ang - iTime / 9.0)*16.0, dist*32.0));

	float f0 = 0.0;
	//f0 = 1.0/(length(uv-flare_pos)*16.0+1.0);

	//f0 = f0+f0*(sin((ang+iTime/18.0 + noise(abs(ang)+n/2.0)*2.0)*12.0)*.1+dist*.1+.8);

	// Outermost, biggest flare:
	float f2  = max(1.0 / (1.0 + 32.0*pow(length(uvd + 0.80 * flare_pos), 2.0)), 0.0) * 0.25;
	float f22 = max(1.0 / (1.0 + 32.0*pow(length(uvd + 0.85 * flare_pos), 2.0)), 0.0) * 0.23;
	float f23 = max(1.0 / (1.0 + 32.0*pow(length(uvd + 0.90 * flare_pos), 2.0)), 0.0) * 0.21;

	// Middle flare. 2.4 is the size. Smaller numbers -> smaller size
	vec2 uvx = mix(coord, uvd, -0.5);
	float f4  = max(0.01 - pow(length(uvx + 0.40 * flare_pos), 2.4), 0.0) * 6.0;
	float f42 = max(0.01 - pow(length(uvx + 0.45 * flare_pos), 2.4), 0.0) * 5.0;
	float f43 = max(0.01 - pow(length(uvx + 0.50 * flare_pos), 2.4), 0.0) * 3.0;

	// Middle, bigger and fainter flare:
	uvx = mix(coord, uvd, -0.4);
	float f5  = max(0.01 - pow(length(uvx + 0.2 * flare_pos), 5.5), 0.0) * 2.0;
	float f52 = max(0.01 - pow(length(uvx + 0.4 * flare_pos), 5.5), 0.0) * 2.0;
	float f53 = max(0.01 - pow(length(uvx + 0.6 * flare_pos), 5.5), 0.0) * 2.0;

	uvx = mix(coord, uvd, -0.5);
	float f6  = max(0.01 - pow(length(uvx - 0.300 * flare_pos), 1.6), 0.0) * 6.0;
	float f62 = max(0.01 - pow(length(uvx - 0.325 * flare_pos), 1.6), 0.0) * 3.0;
	float f63 = max(0.01 - pow(length(uvx - 0.350 * flare_pos), 1.6), 0.0) * 5.0;

	// Additional flare:
	uvx = mix(coord, uvd, -0.2);
	float fA  = max(0.01 - pow(length(uvx + 0.70 * flare_pos), 2.0), 0.0) * 4.0;
	float fA2 = max(0.01 - pow(length(uvx + 0.75 * flare_pos), 2.0), 0.0) * 5.0;
	float fA3 = max(0.01 - pow(length(uvx + 0.80 * flare_pos), 2.0), 0.0) * 4.5;

	vec3 c = 0.0;
	c.r += f2  + f4  + f5  + f6  + fA; 
	c.g += f22 + f42 + f52 + f62 + fA2; 
	c.b += f23 + f43 + f53 + f63 + fA3;
	//c+=vec3(f0);

	// Add hexagon lens flares
	for (float i = 0.0; i < 5.0; i++) {
		float dist = rnd(i * 20.0) * 3.0 + 0.2;
		vec3 regColor = cos(vec3(0.44, 0.24, 0.2) * 8.0 + i * 4.0) * 0.5 + 0.5;
		float s = max(0.01 - pow(regShape(coord * 5.0 + flare_pos * dist*5.0 + 0.9, 6), 1.0), 0.0);
		c += s * regColor;
	}

	// Add a horizontal flare
	vec3 flareColor = lerp(1.0, SunColor[sun_index].rgb, SunColor[sun_index].w);
	c += flareColor * anamorphicFlare(coord - flare_pos);

	/*
	// Add hexagonal rays
	float L = exp(-length(coord - flare_pos) * 5.0);
	vec2 hexflare = foldHex(coord - flare_pos);
	float f = 0.025 / hexflare.x * L;
	c += flareColor * 0.05 * f;
	*/

	// Add hexagonal rays
	//c += 3.5 * 3.0 * flareColor * thickRays(coord, flare_pos, 0.05) * multiRays(coord, flare_pos, 5.7, 5.2, 0.1);

	return c;
}

/*
vec3 cc(vec3 color, float factor, float factor2) // color modifier
{
	float w = color.x + color.y + color.z;
	return mix(color, w * factor, w * factor2);
}
*/

float sdCircle(in vec2 p, float radius)
{
	return length(p) - radius;
}

/*
float sdLine(in vec2 p, in vec2 a, in vec2 b)
{
	vec2 pa = p - a, ba = b - a;
	float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
	return length(pa - ba * h);
}
*/

PixelShaderOutput main(PixelShaderInput input) {
	PixelShaderOutput output;
	vec2 fragCoord = input.uv * iResolution.xy;
	vec3 sunPos3D, color = 0.0;
	//float3 pos3D = depthTex.Sample(depthSampler, input.uv).xyz;
	vec2 sunPos = 0.0;
	float d, dm;
	if (VRmode == 1)
		output.color = float4(0, 0, 0, 1);
	else
#ifdef GENMIPMAPS_DEBUG
		// DEBUG section. This will enable mip maps in this shader
		output.color = bgTex.SampleLevel(bgSampler, input.uv, 5.0);
#else
#ifdef INSTANCED_RENDERING
        output.color = bgTex.Sample(bgSampler, float3(input.uv, input.viewId));
#else
        output.color = bgTex.Sample(bgSampler, input.uv);
#endif
#endif

	// Early exit: avoid rendering outside the original viewport edges
	if (any(input.uv < p0) || any(input.uv > p1))
		return output;

	// Notice how in this shader we don't care about FOVscale or y_center. That's because
	// it's using in-game UV coords
	vec2 p = (2.0 * fragCoord.xy - iResolution.xy) / min(iResolution.x, iResolution.y);
	//p += vec2(0, y_center); // Use this for light vectors, In XWA the aiming HUD is not at the screen's center in cockpit view
	//vec3 v = vec3(p.x, -p.y, -FOVscale); // Use this for light vectors
	
	//p *= preserveAspectRatioComp;
	//vec3 v = vec3(p, -FOVscale);
	//vec3 v = float3(p, 0);

	//v = mul(viewMat, vec4(v, 0.0)).xyz;
	//vec3 v = vec3(p, 0.0);
	//vec2 sunPos = (vec2(SunXL, SunYL) - 0.5) * 2.0;
	
	// Use the following then SunXL, SunYL is in desktop-resolution coordinates:
	//vec2 sunPos = (2.0 * vec2(SunXL, SunYL) - iResolution.xy) / min(iResolution.x, iResolution.y);
	
	// Use the following when SunXL,YL is a light vector:
	//vec2 sunPos = -2.35 * vec2(-SunXL.x, SunYL);
	//vec2 sunPos = debugFOV * vec2(-SunXL.x, SunYL);

	// Non-VR path: coords are already 2D
	if (VRmode == 0) {
		// sunPos is (0,0) at the screen center and is in the range -1..1 for each axis
		sunPos = (2.0 * SunCoords[0].xy - iResolution.xy) / min(iResolution.x, iResolution.y);
		// Sample the depth at the location of the sun:
		sunPos3D = depthTex.Sample(depthSampler, SunCoords[0].xy / iResolution.xy).xyz;
	}
	else {
		// DirectSBS path: we'll sample the depth buffer in SunFlareCompose. The reason is that the
		// SunCoords are still 2D, but they are in-game UV coords, not post-proc UV coords, so they
		// can't be used to sample the depth buffer here.
		sunPos = SunCoords[0].xy; // 2D coord pass-through
		sunPos3D.z = INFINITY_Z + 500; // Compute the right depth value later, in SunFlareCompose
	}

	// Avoid displaying any flare if the sun is occluded:
	if (sunPos3D.z < INFINITY_Z)
		return output;

	// Now let's sample the stellar background color at the location where the sun was rendered.
	// If the color isn't bright enough, the sun was obscured by something else (e.g. a thick cloud)
	// and we can inhibit the flare.
	// TODO: Enable the VR path
#ifdef INSTANCED_RENDERING
	const float4 backdropSample = stellarBckTex.Sample(bgSampler, float3(SunCoords[0].xy / iResolution.xy, input.viewId));
#else
	const float4 backdropSample = stellarBckTex.Sample(bgSampler, SunCoords[0].xy / iResolution.xy);
#endif
	// This is the total backdrop brightness level, we can use this to mask the flare:
	const float backdropLevel = dot(backdropSample.rgb, /* approx luma */ float3(0.299, 0.587, 0.114));
	if (backdropLevel < 0.8)
		return output;

#undef FLARE_DEBUG
#ifndef FLARE_DEBUG
	// The aspect_ratio_comp factor was added for the DirectSBS mode, but while it fixes the aspect
	// ratio of the flares, it might mess up the position of the centroid.
	//const float2 aspect_ratio_comp = float2(1.0, mr_vr_aspect_ratio);
	//output.color.rgb += flare_intensity * lensflare(aspect_ratio_comp * p.xy, aspect_ratio_comp * sunPos, 0);
	output.color.rgb += flare_intensity * lensflare(p.xy, sunPos, 0);
#else
	// DEBUG: Draw a reticle instead of the flare: it should appear on top of the sun
	const float2 aspect_ratio_comp = float2(1.0, mr_vr_aspect_ratio);
	float3 col = float3(0.1, 1.0, 0.1); // Marker color
	//float2 aspect_ratio = VRmode != 0 ? float2(iResolution.y / iResolution.x, iResolution.x / iResolution.y) : 1.0;
	//d = sdCircle(aspect_ratio * (p.xy - sunPos), 0.05);
	//d = sdCircle(float2(mr_debug_value, 1.0) * (p.xy - sunPos), 0.05);

	//d = sdCircle((p.xy - sunPos), 0.05);
	d = sdCircle(aspect_ratio_comp * (p.xy - sunPos), 0.05);
	dm = smoothstep(thickness * 0.5, 0.0, abs(d)); // Outer ring
	dm += smoothstep(thickness * 0.5, 0.0, abs(d + 0.5 * scale * (cursor_radius - 0.001))); // Center dot
	col *= dm;
	output.color.rgb = lerp(output.color.rgb, col, 0.8 * dm);
#endif

	return output;
}
