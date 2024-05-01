// Noise animation - Lava
// by nimitz (twitter: @stormoid)
// https://www.shadertoy.com/view/lslXRS
// License Creative Commons Attribution-NonCommercial-ShareAlike 3.0 Unported License
// Tiling from https://www.shadertoy.com/view/Md2yWh by noby
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

#define LavaSize iResolution.x
#define LavaBloom iResolution.y
#define LavaColor SunColor[0]
#define LavaTiling Style
// DEBUG properties, remove later
//#define LavaNormalMult SunColor[1]
//#define LavaPosMult SunColor[2]
//#define LavaTranspose bDisneyStyle

struct PixelShaderInput
{
	float4 pos    : SV_POSITION;
	float4 pos3D  : COLOR1;
	float4 normal : NORMAL;
	float2 tex	  : TEXCOORD;
	//float4 color  : COLOR0;
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
		p += time * 0.3;
		//p += time * 6.0;

		// secondary flow speed (speed of the perceived flow)
		//bp += time * 1.9;
		bp += time * 1.1;

		// displacement field (try changing time multiplier)
		vec2 gr = gradn(i * p * 0.34 + time * 1.0);

		// rotation of the displacement field
		//gr *= makem2(time*6. - (0.05*p.x + 0.03*p.y)*40.);
		gr = gradn(i * p * 0.34);

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

/*
// From http://www.thetenthplanet.de/archives/1180
mat3 cotangent_frame(vec3 N, vec3 p, vec2 uv) { 
	// get edge vectors of the pixel triangle
	vec3 dp1 = dFdx( p ); 
	vec3 dp2 = dFdy( p ); 
	vec2 duv1 = dFdx( uv ); 
	vec2 duv2 = dFdy( uv );   
	
	// solve the linear system
	vec3 dp2perp = cross( dp2, N );
	vec3 dp1perp = cross( N, dp1 );
	vec3 T = dp2perp * duv1.x + dp1perp * duv2.x;
	vec3 B = dp2perp * duv1.y + dp1perp * duv2.y;
	
	// construct a scale-invariant frame
	float invmax = inversesqrt( max( dot(T,T), dot(B,B) ) );
	return mat3( T * invmax, B * invmax, N );
}

vec3 perturb_normal(vec3 N, vec3 V, vec2 texcoord) {
	// assume N, the interpolated vertex normal and
	// V, the view vector (vertex to eye)
	vec3 map = texture2D( mapBump, texcoord ).xyz;
#ifdef WITH_NORMALMAP_UNSIGNED
	map = map * 255./127. - 128./127.;
#endif
#ifdef WITH_NORMALMAP_2CHANNEL
	map.z = sqrt( 1. - dot( map.xy, map.xy ) );
#endif
#ifdef WITH_NORMALMAP_GREEN_UP
	map.y = -map.y;
#endif
	mat3 TBN = cotangent_frame( N, -V, texcoord ); // V is the view vector, but this is equivalent to the vertex pos
	return normalize( TBN * map );
}

varying vec3 g_vertexnormal;
varying vec3 g_viewvector; // camera pos - vertex pos
varying vec2 g_texcoord;

void main() {
	vec3 N = normalize( g_vertexnormal );  
#ifdef WITH_NORMALMAP
	N = perturb_normal( N, g_viewvector, g_texcoord );
#endif   // ...
}

// Parallax Mapping, from: https://www.gamedev.net/tutorials/_/technical/graphics-programming-and-theory/a-closer-look-at-parallax-occlusion-mapping-r3262/

while ( nCurrSample < nNumSamples ) {
	fCurrSampledHeight = NormalHeightMap.SampleGrad( LinearSampler, IN.texcoord + vCurrOffset, dx, dy ).a;

	// The ray's height is fCurrRayHeight
	// The sampled height is fCurrSampledHeight
	// If the sampled height is higher than the ray, then the current point
	// along the ray is below the height map, we need to find the intersection
	// between the ray and the line made by the last two samples
	if ( fCurrSampledHeight > fCurrRayHeight ) {
		float delta1 = fCurrSampledHeight - fCurrRayHeight;
		float delta2 = ( fCurrRayHeight + fStepSize ) - fLastSampledHeight;
		float ratio = delta1/(delta1+delta2);
		vCurrOffset = (ratio) * vLastOffset + (1.0-ratio) * vCurrOffset; // <-- Solution
		nCurrSample = nNumSamples + 1; // <-- Exit
	} else {
		nCurrSample++; fCurrRayHeight -= fStepSize;
		vLastOffset = vCurrOffset;
		vCurrOffset += fStepSize * vMaxOffset;
		fLastSampledHeight = fCurrSampledHeight;
	}
}


float2 vFinalCoords = IN.texcoord + vCurrOffset;
float4 vFinalNormal = NormalHeightMap.Sample( LinearSampler, vFinalCoords ); //.a;
float4 vFinalColor = ColorMap.Sample( LinearSampler, vFinalCoords ); // Expand the final normal vector from [0,1] to [-1,1] range. vFinalNormal = vFinalNormal * 2.0f - 1.0f;

*/

// In the shadertoy lava example from https://www.shadertoy.com/view/wscfW7 the coord sys looks like this:
// x+ = right
// y+ = up
// z+ = towards the camera/screen
//
// Normals:
// x+ = right
// y+ = up
// z+ = towards the camera/screen
//
// In the shadertoy example, ro (ray's origin) is at [0,0,2]
//
// In XWA the coord sys looks like this:
// x+ = right
// y+ = up
// z+ = --away from the camera--

// In XWA the normals look like this:
// x+ = right
// y+ = up
// z+ = towards the camera

// From: http://www.thetenthplanet.de/archives/1180
mat3 cotangent_frame(vec3 N, vec3 p, vec2 uv, out vec3 T, out vec3 B) {
	// get edge vectors of the pixel triangle
	vec3 dp1 = ddx(p);
	vec3 dp2 = ddy(p);
	vec2 duv1 = ddx(uv);
	vec2 duv2 = ddy(uv);

	// solve the linear system
	vec3 dp2perp = cross(dp2, N);
	vec3 dp1perp = cross(N, dp1);
	T = dp2perp * duv1.x + dp1perp * duv2.x;
	B = dp2perp * duv1.y + dp1perp * duv2.y;

	// construct a scale-invariant frame
	float invmax = rsqrt(max(dot(T, T), dot(B, B)));
	return mat3(T * invmax, B * invmax, N);
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
		bp += 2.0 * sc.yx;

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

// Tiling code starts here:
static float F_CONST = 0.5*(1.0 + sqrt(2.0));

float lerpx(in vec2 uv, in float repeat) {
	float v  = flow(uv + vec2(-repeat * 0.5, 0)) * uv.x / repeat;
	      v += flow(uv + vec2( repeat * 0.5, 0)) * (repeat - uv.x) / repeat;
	// overlaying non-correlated noise reduces variance around repetition edges
	// factor f scales it back
	return mix(v, F_CONST * pow(abs(v), F_CONST),
			   4.0 * (uv.x / repeat) * (repeat - uv.x) / repeat);
}

float lerpy(in vec2 uv, in float repeat) {
	float v  = lerpx(uv + vec2(0, -repeat * 0.5), repeat) * uv.y / repeat;
	      v += lerpx(uv + vec2(0,  repeat * 0.5), repeat) * (repeat - uv.y) / repeat;
	// same scaling in y dimension
	return mix(v, F_CONST * pow(abs(v), F_CONST),
		       4.0 * (uv.y / repeat) * (repeat - uv.y) / repeat);
}

float noisetile(in vec2 uv, in float repeat) {
	return lerpy(uv, repeat);
}


// Parallax Mapping. Algorithm from:
// https://www.gamedev.net/tutorials/_/technical/graphics-programming-and-theory/a-closer-look-at-parallax-occlusion-mapping-r3262/
// See a clean implementation here: https://www.shadertoy.com/view/wscfW7
//
// Input: 
//		pos: 3D position.
//		N: (normalized) normal.
//		uv: The uv coord for this point.
// Additional inputs unrelated to parallax mapping:
//		repeat
//
// Returns: the final sample
vec3 parallax(vec3 pos, vec3 N, vec2 uv, float repeat)
{
	const float fHeightMapScale = 3.5;
	const float nMaxSamples = 100.0;
	const float nMinSamples = 15.0;
	vec3 T, B;

	//vec3 FN = N.xyz * LavaNormalMult.xyz;
	vec3 FN = N.xyz;
	mat3 TBN = cotangent_frame(FN, -pos, uv, T, B);

	//if (LavaTranspose)
		TBN = transpose(TBN);

	vec3 eye = mul(TBN, pos);
	vec3 normal = mul(TBN, FN);

	float fParallaxLimit = -length(eye.xy) / eye.z;
	fParallaxLimit *= fHeightMapScale;

	vec2 vOffsetDir = 0.075 * normalize(eye.xy);
	//vec2 vOffsetDir = 0.01 * normalize(eye.xy);
	vec2 vMaxOffset = vOffsetDir * fParallaxLimit;
	float nNumSamples = mix(nMaxSamples, nMinSamples, dot(eye, normal));
	float fStepSize = 1.0 / nNumSamples;

	float fCurrRayHeight = 1.0;
	vec2  vCurrOffset = 0.0;
	vec2  vLastOffset = 0.0;
	float fLastSampledHeight = 1.0;
	float fCurrSampledHeight = 1.0;
	float nCurrSample = 0.0;

	while (nCurrSample < nNumSamples) {
		//fCurrSampledHeight = noisetile(uv + vCurrOffset, repeat);
		fCurrSampledHeight = flow(uv + vCurrOffset);
		//fCurrSampledHeight = 0.05 / flow(uv + vCurrOffset);
		//fCurrSampledHeight = dot(vec3(0.333), texture(iChannel0, uv + vCurrOffset).rgb);
		//fCurrSampledHeight = NormalHeightMap.SampleGrad( LinearSampler, IN.texcoord + vCurrOffset, dx, dy ).a;

		// The ray's height is fCurrRayHeight
		// The sampled height is fCurrSampledHeight
		// If the sampled height is higher than the ray, then the current point
		// along the ray is below the height map, we need to find the intersection
		// between the ray and the line made by the last two samples
		if (fCurrSampledHeight > fCurrRayHeight) {
			float delta1 = fCurrSampledHeight - fCurrRayHeight;
			float delta2 = (fCurrRayHeight + fStepSize) - fLastSampledHeight;
			float ratio = delta1 / (delta1 + delta2 + 0.001);
			vCurrOffset = ratio * vLastOffset + (1.0 - ratio) * vCurrOffset; // <-- Solution
			break;
		}
		else {
			nCurrSample++;
			fCurrRayHeight -= fStepSize;
			vLastOffset = vCurrOffset;
			vCurrOffset += fStepSize * vMaxOffset;
			fLastSampledHeight = fCurrSampledHeight;
		}
	}
	// return uv + vCurrOffset;
	//return noisetile(frac(uv + vCurrOffset), repeat);
	vec2 finaluv = uv + vCurrOffset;
	if (any(finaluv) < 0.0)
		return vec3(0, 0, 1);
	if (any(finaluv) > 1.0)
		return vec3(0, 1, 0);
	return vec3(0.2, 0.07, 0.01) / flow(uv + vCurrOffset);
}

PixelShaderOutput main(PixelShaderInput input)
{
	PixelShaderOutput output;
	float4 texelColor = texture0.Sample(sampler0, input.tex);
	float  alpha = texelColor.w;
	float3 P = input.pos3D.xyz;
	float  SSAOAlpha = saturate(min(alpha - fSSAOAlphaOfs, fPosNormalAlpha));
	vec2 p;

	// Zero-out the bloom mask.
	output.bloom = 0;
	output.color = texelColor;
	output.pos3D = float4(P, SSAOAlpha);
	output.ssMask = 0;

	// hook_normals code. In this shader we zero-out the normal in the output, because this is
	// a shadeless effect. However, we *do* need the Normal in this shader to compute parallax
	// mapping.
	// TOOD: The new D3DRendererHook provides the OPT -> Viewspace transform matrix, we can probably
	// do parallax mapping with that matrix now
	float3 N = normalize(input.normal.xyz);
	N.yz = -N.yz; // Invert the Y axis, originally Y+ is down
	//output.normal = 0; // float4(N, SSAOAlpha);
	output.normal = float4(N, 1); // float4(N, SSAOAlpha);

	// ssaoMask: Material, Glossiness, Specular Intensity
	output.ssaoMask = float4(fSSAOMaskVal, fGlossiness, fSpecInt, alpha);
	// SS Mask: Glass, Specular Value, Shadeless
	output.ssMask = float4(0, fSpecVal, 1.0, alpha);

	float repeat = 2.0 + (LavaSize - 1.0);
	float size = 1.0 + LavaSize;
	float rz = 0.0;
	if (LavaTiling) {
		// Fix UVs that are greater than 1 or negative.
		p = frac(input.tex.xy);
		if (p.x < 0.0) p.x += 1.0;
		if (p.y < 0.0) p.y += 1.0;

		p = mod(p * size, repeat);
		rz = 0.2 / noisetile(p, repeat);

		//vec3 eye = vec3(0.0, 0.0, 0.0);
		//vec3 pos = LavaPosMult.xyz * output.pos3D.xyz;
		//vec3 eye_vec = normalize(eye - pos);
		//float rz = 1.0 / parallax(eye_vec, N, p, repeat);
	}
	else {
		p = input.tex.xy;
		p *= 3.0 * LavaSize;
		rz = 0.2 / flow(p);
	}

	vec3 col = LavaColor.xyz * rz;
	output.color = vec4(col, 1.0);
	// Make the bloom sharper:
	rz *= rz;
	rz *= rz;
	rz = clamp(rz, 0.0, 1.0);
	output.bloom = LavaBloom * rz * float4(col, rz);
	//output.bloom = 0.0;
	//float n = noise(input.tex.xy * 100.0);
	//float n = noiseTex.Sample(noiseSamp, input.tex.xy).x;
	//output.color = vec4(n, n, n, 1);

	// Original code:
	//output.color = float4(brightness * diffuse * texelColor.xyz, texelColor.w);
	return output;
}
