// Noise animation - Lava
// by nimitz (twitter: @stormoid)
// https://www.shadertoy.com/view/lslXRS
// License Creative Commons Attribution-NonCommercial-ShareAlike 3.0 Unported License
// Contact the author for other licensing options
// Adapted to XWA by Leo Reyes

#include "shader_common.h"
#include "HSV.h"
#include "shading_system.h"
#include "PixelShaderTextureCommon.h"

#include "ShaderToyDefs.h"
#include "ShadertoyCBuffer.h"

// The base texture. We can probably use this as an alpha mask to blend the lava effect with
// this texture?
Texture2D    texture0 : register(t0);
SamplerState sampler0 : register(s0);

// Gray noise texture
Texture2D	 noiseTex  : register(t1);
SamplerState noiseSamp : register(s1);

struct PixelShaderInput
{
	float4 pos    : SV_POSITION;
	float4 color  : COLOR0;
	float2 tex    : TEXCOORD0;
	float4 pos3D  : COLOR1;
	float4 normal : NORMAL;
};

struct PixelShaderOutput
{
	float4 color    : SV_TARGET0;
	float4 bloom    : SV_TARGET1;
	float4 pos3D    : SV_TARGET2;
	float4 normal   : SV_TARGET3;
	float4 ssaoMask : SV_TARGET4;
	float4 ssMask   : SV_TARGET5;
};

mat2 makem2(in float theta) { 
	float2 sc;
	
	sincos(theta, sc.x, sc.y);
	//return mat2(c, -s, s, c); 
	return mat2(sc.y, -sc.x, sc.x, sc.y);
}

float noise(in vec2 x) 
{
	return noiseTex.SampleLevel(noiseSamp, x * 0.01, 0).r;
}

vec2 gradn(vec2 p)
{
	float ep = 0.09;
	//float ep = 0.04;
	float gradx = noise(vec2(p.x + ep, p.y)) - noise(vec2(p.x - ep, p.y));
	float grady = noise(vec2(p.x, p.y + ep)) - noise(vec2(p.x, p.y - ep));
	return vec2(gradx, grady);
}

float flow(vec2 p)
{
	float time = iTime * 0.1;
	float z = 2.0;
	float rz = 0.0;
	vec2 bp = p;
	//mat2 m = makem2(time * 6.0 - (0.05 * p.x + 0.03 * p.y) * 40.0);
	mat2 m = makem2(time * 6.0 - (0.05 * p.x + 0.03 * p.y) * 10.0);
	vec2 sc;

	sincos(time, sc.x, sc.y);

	for (float i = 1.0; i < 7.0; i++)
	{
		p += sc;
		bp += sc.yx;

		// primary flow speed
		p += time * 0.6;
		//p += time * 6.0;

		// secondary flow speed (speed of the perceived flow)
		bp += time * 1.9;

		// displacement field (try changing time multiplier)
		vec2 gr = gradn(i * p * 0.34 + time * 1.0);

		// rotation of the displacement field
		//gr *= makem2(time*6. - (0.05*p.x + 0.03*p.y)*40.);
		gr = mul(m, gr);

		// displace the system
		p += gr * 0.5;

		// add noise octave
		rz += (sin(noise(p) * 7.0) * 0.5 + 0.5) / z;

		// blend factor (blending displaced system with base system)
		// you could call this advection factor (.5 being low, .95 being high)
		p = mix(bp, p, 0.77);

		// intensity scaling
		z *= 1.8;
		// octave scaling
		p *= 2.0;
		bp *= 1.9;
	}
	return rz;
}

float flow_new(vec2 p)
{
	float time = iTime * 0.1;
	float z = 2.0;
	float rz = 0.0;
	vec2 bp = p;
	//mat2 m = makem2(time * 6.0 - (0.05 * p.x + 0.03 * p.y) * 40.0);
	mat2 m = makem2(time * 6.0 - (0.05 * p.x + 0.03 * p.y) * 10.0);
	vec2 sc;

	sincos(time, sc.x, sc.y);

	for (float i = 1.0; i < 7.0; i++)
	{
		p += sc;
		bp += sc.yx;

		// displacement field
		vec2 gr = gradn(i * p * 0.34);

		// rotation of the displacement field
		gr = mul(m, gr);

		// displace the system
		p += gr * 0.5;

		// add noise octave
		rz += (sin(noise(p) * 7.0) * 0.5 + 0.5) / z;

		// blend factor (blending displaced system with base system)
		// you could call this advection factor (.5 being low, .95 being high)
		p = mix(bp, p, 0.77);

		// intensity scaling
		z *= 1.8;
		// octave scaling
		p *= 2.0;
		bp *= 1.9;
	}
	return rz;
}

PixelShaderOutput main(PixelShaderInput input)
{
	PixelShaderOutput output;
	float4 texelColor = texture0.Sample(sampler0, input.tex);
	float  alpha = texelColor.w;
	float3 diffuse = lerp(input.color.xyz, 1.0, fDisableDiffuse);
	float3 P = input.pos3D.xyz;
	float  SSAOAlpha = saturate(min(alpha - fSSAOAlphaOfs, fPosNormalAlpha));
	// Zero-out the bloom mask.
	output.bloom = 0;
	output.color = texelColor;
	output.pos3D = float4(P, SSAOAlpha);
	output.ssMask = 0;
	// Here we're co-opting iResolution.x for the lava size
#define LavaSize iResolution.x
#define BloomStrength iResolution.y

	// hook_normals code:
	//float3 N = normalize(input.normal.xyz * 2.0 - 1.0);
	//N.y = -N.y; // Invert the Y axis, originally Y+ is down
	//N.z = -N.z;

	output.normal = 0; //  float4(N, SSAOAlpha);

	// SSAO Mask/Material, Glossiness, Spec_Intensity
	// Glossiness is multiplied by 128 to compute the exponent
	//output.ssaoMask = float4(fSSAOMaskVal, DEFAULT_GLOSSINESS, DEFAULT_SPEC_INT, alpha);
	// ssaoMask.r: Material
	// ssaoMask.g: Glossiness
	// ssaoMask.b: Specular Intensity
	output.ssaoMask = float4(fSSAOMaskVal, fGlossiness, fSpecInt, alpha);
	// SS Mask: Normal Mapping Intensity, Specular Value, Shadeless
	output.ssMask = float4(fNMIntensity, fSpecVal, 1.0, alpha);

	//vec2 fragCoord = input.tex.xy * iResolution.xy;
	//vec2 p = fragCoord.xy / iResolution.xy - 0.5;
	//p.x *= iResolution.x / iResolution.y;

	vec2 p = input.tex.xy;

	// Fix UVs that are greater than 1 or negative.
	//vec2 p = frac(input.tex.xy);
	//if (p.x < 0.0) p.x += 1.0;
	//if (p.y < 0.0) p.y += 1.0;
	//p = p - 0.5;

	p *= 3.0 * LavaSize;
	//output.color = vec4(flow(p), 0.0, 1.0);

	float rz = 1.0 / flow(p);
	vec3 col = vec3(0.2, 0.07, 0.01) * rz;
	output.color = vec4(col, 1.0);
	output.bloom = float4(col, BloomStrength * rz);
	//float n = noise(input.tex.xy * 100.0);
	//float n = noiseTex.Sample(noiseSamp, input.tex.xy).x;
	//output.color = vec4(n, n, n, 1);

	// Original code:
	//output.color = float4(brightness * diffuse * texelColor.xyz, texelColor.w);
	return output;
}
