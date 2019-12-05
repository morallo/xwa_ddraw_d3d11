/*
 * Hyperspace Tunnel
 * Based on theGiallo's shader: https://www.shadertoy.com/view/MttSz2
 */

#include "ShaderToyDefs.h"

static const float3 blue_color = float3(0.15, 0.35, 0.8);
static const float period = 2.5; // 2.7;
static const float rotation_speed = 2.3;
static const float speed = 5.5; // 5.0;
static const float t2 = 4.0;

// The Foreground Color Buffer (_shadertoyBuf)
Texture2D fgColorTex : register(t0);
SamplerState fgColorSampler : register(s0);

// The Background Color Buffer (_shadertoyAuxBuf)
Texture2D bgColorTex : register(t1);
SamplerState bgColorSampler : register(s1);

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

PixelShaderOutput main(PixelShaderInput input)
{
	PixelShaderOutput output;
	vec4 fragColor = vec4(0.0, 0.0, 0.0, 1);
	vec2 fragCoord = input.uv * iResolution.xy;
	
	vec2 q = fragCoord.xy / iResolution.xy;
	vec2 qc = 2.0 * q - 1.0;
	vec2 p = (2.0 * fragCoord.xy - iResolution.xy) / min(iResolution.y, iResolution.x);
	vec4 col = vec4(0, 0, 0, 1);
	output.bloom = 0;

	// Early exit: avoid rendering outside the original viewport edges
	if (input.uv.x < x0 || input.uv.x > x1 ||
		input.uv.y < y0 || input.uv.y > y1)
	{
		output.color = 0.0;
		return output;
	}

	float4 fgcol = fgColorTex.Sample(fgColorSampler, input.uv);
	float t = mod(iTime, t2) / t2; // Normalized time
	// The focal_depth controls how "deep" the tunnel looks. Lower values
	// provide more depth.
	float focal_depth = mix(0.15, 0.015, smoothstep(0.65, 1.0, t));

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

	// Colorize blue
	float val = fBm(0.75 * cp);
	col.rgb = val * blue_color;

	// Add white spots
	float white_spot = 0.3 * smoothstep(0.65, 1.0, val);
	col.rgb += white_spot;
	output.bloom = white_spot;
	output.bloom.rgb *= 2.5;

	float w_total = 0.0, w_out = 0.0, w_in;
	// Fade in and out from white every t2 seconds
	w_in = abs(1.0 - 1.0 * smoothstep(0.0, 0.25, t));
	w_out = abs(1.0 * smoothstep(0.8, 1.0, t));
	w_total = max(w_in, w_out);

	// Add the white disk at the center
	//float disk_size = 0.125;
	float disk_size = max(0.11, 0.5 * w_out);
	float disk_col = exp(-(dp_len - disk_size) * 3.0);
	col.rgb += disk_col;
	
	// Whiteout
	col.rgb = mix(col.rgb, 1.0, w_total);

	// Mask the bloom
	output.bloom *= (1.0 - fgcol.a);

	fragColor = vec4(col.rgb, 1);
	output.color = fragColor;
	return output;
}
