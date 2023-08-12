/*
From: https://www.shadertoy.com/view/ls3GWS
and http://www.geeks3d.com/20110405/fxaa-fast-approximate-anti-aliasing-demo-glsl-opengl-test-radeon-geforce/3/
*/
#include "ShaderToyDefs.h"
#include "ShadertoyCBuffer.h"

// The texture to apply AA to
#ifdef INSTANCED_RENDERING
Texture2DArray colorTex  : register(t0);
#else
Texture2D       colorTex : register(t0);
#endif
SamplerState   colorSamp : register(s0);

#define FXAA_SPAN_MAX 8.0
#define FXAA_REDUCE_MUL   (1.0/FXAA_SPAN_MAX)
#define FXAA_REDUCE_MIN   (1.0/128.0)
#define FXAA_SUBPIX_SHIFT (1.0/4.0)

struct PixelShaderInput
{
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD;
#ifdef INSTANCED_RENDERING
	uint   viewId : SV_RenderTargetArrayIndex;
#endif
};

struct PixelShaderOutput
{
	float4 color : SV_TARGET0;
};

PixelShaderOutput main(PixelShaderInput input)
{
	PixelShaderOutput output;
	output.color = 0;

	vec2 rcpFrame = 1.0 / iResolution.xy;
	vec2 uv2 = input.uv - (rcpFrame * (0.5 + FXAA_SUBPIX_SHIFT));

	// DEBUG
	//output.color = colorTex.Sample(colorSamp, input.uv);
	//output.color.b += 0.1;
	//return output;
	// DEBUG

#ifdef INSTANCED_RENDERING
	vec3 rgbNW = colorTex.Sample(colorSamp, float3(uv2, input.viewId), 0.0).xyz;
	vec3 rgbNE = colorTex.Sample(colorSamp, float3(uv2 + vec2(1, 0) * rcpFrame.xy, input.viewId), 0.0).xyz;
	vec3 rgbSW = colorTex.Sample(colorSamp, float3(uv2 + vec2(0, 1) * rcpFrame.xy, input.viewId), 0.0).xyz;
	vec3 rgbSE = colorTex.Sample(colorSamp, float3(uv2 + vec2(1, 1) * rcpFrame.xy, input.viewId), 0.0).xyz;
	vec3 rgbM  = colorTex.Sample(colorSamp, float3(input.uv, input.viewId), 0.0).xyz;
#else
	vec3 rgbNW = colorTex.Sample(colorSamp, uv2, 0.0).xyz;
	vec3 rgbNE = colorTex.Sample(colorSamp, uv2 + vec2(1, 0) * rcpFrame.xy, 0.0).xyz;
	vec3 rgbSW = colorTex.Sample(colorSamp, uv2 + vec2(0, 1) * rcpFrame.xy, 0.0).xyz;
	vec3 rgbSE = colorTex.Sample(colorSamp, uv2 + vec2(1, 1) * rcpFrame.xy, 0.0).xyz;
	vec3 rgbM  = colorTex.Sample(colorSamp, input.uv, 0.0).xyz;
#endif

	vec3 luma = vec3(0.299, 0.587, 0.114);
	float lumaNW = dot(rgbNW, luma);
	float lumaNE = dot(rgbNE, luma);
	float lumaSW = dot(rgbSW, luma);
	float lumaSE = dot(rgbSE, luma);
	float lumaM = dot(rgbM, luma);

	float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
	float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));

	vec2 dir;
	dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
	dir.y = ((lumaNW + lumaSW) - (lumaNE + lumaSE));

	float dirReduce = max(
		(lumaNW + lumaNE + lumaSW + lumaSE) * (0.25 * FXAA_REDUCE_MUL),
		FXAA_REDUCE_MIN);
	float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);

	dir = min(vec2(FXAA_SPAN_MAX, FXAA_SPAN_MAX),
		max(vec2(-FXAA_SPAN_MAX, -FXAA_SPAN_MAX),
			dir * rcpDirMin)
	) * rcpFrame.xy;

#ifdef INSTANCED_RENDERING
	vec3 rgbA = (1.0 / 2.0) * (
		colorTex.Sample(colorSamp, float3(input.uv + dir * (1.0 / 3.0 - 0.5), input.viewId), 0.0).xyz +
		colorTex.Sample(colorSamp, float3(input.uv + dir * (2.0 / 3.0 - 0.5), input.viewId), 0.0).xyz);
	vec3 rgbB = rgbA * (1.0 / 2.0) + (1.0 / 4.0) * (
		colorTex.Sample(colorSamp, float3(input.uv + dir * (0.0 / 3.0 - 0.5), input.viewId), 0.0).xyz +
		colorTex.Sample(colorSamp, float3(input.uv + dir * (3.0 / 3.0 - 0.5), input.viewId), 0.0).xyz);
#else
	vec3 rgbA = (1.0 / 2.0) * (
		colorTex.Sample(colorSamp, input.uv + dir * (1.0 / 3.0 - 0.5), 0.0).xyz +
		colorTex.Sample(colorSamp, input.uv + dir * (2.0 / 3.0 - 0.5), 0.0).xyz);
	vec3 rgbB = rgbA * (1.0 / 2.0) + (1.0 / 4.0) * (
		colorTex.Sample(colorSamp, input.uv + dir * (0.0 / 3.0 - 0.5), 0.0).xyz +
		colorTex.Sample(colorSamp, input.uv + dir * (3.0 / 3.0 - 0.5), 0.0).xyz);
#endif

	float lumaB = dot(rgbB, luma);

	if ((lumaB < lumaMin) || (lumaB > lumaMax))
		output.color = vec4(rgbA, 1);
	else
		output.color = vec4(rgbB, 1);

	// Something like the following can be used to modulate the effect:
	// output.color.rgb = lerp(rgbM, output.color.rgb, 0.5); 
	return output;
}
