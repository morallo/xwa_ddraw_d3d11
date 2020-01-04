/*
 * Laser Pointer for VR
 * (c) Leo Reyes, 2020.
 */
#include "ShaderToyDefs.h"

static const vec3 lig = normalize(vec3(0.5, 0.0, -0.7));

float sdSphere(in vec3 p, in vec3 center, float radius)
{
	return length(p - center) - radius;
}

//=====================================================

float map(in vec3 p)
{
	return sdSphere(p, vec3(0.0, 0.0, 0.0), 0.05);
}

float intersect(in vec3 ro, in vec3 rd)
{
	const float maxd = 10.0;
	float h = 1.0;
	float t = 0.0;
	for (int i = 0; i < 50; i++)
	{
		if (h<0.001 || t>maxd) break;
		h = map(ro + rd * t);
		t += h;
	}

	if (t > maxd) t = -1.0;

	return t;
}

vec3 calcNormal(in vec3 pos)
{
	vec3 eps = vec3(0.002, 0.0, 0.0);

	return normalize(vec3(
		map(pos + eps.xyy) - map(pos - eps.xyy),
		map(pos + eps.yxy) - map(pos - eps.yxy),
		map(pos + eps.yyx) - map(pos - eps.yyx)));
}

struct PixelShaderInput
{
	float4 pos    : SV_POSITION;
	float4 color  : COLOR0;
	float2 uv     : TEXCOORD0;
	float4 pos3D  : COLOR1;
};

struct PixelShaderOutput
{
	float4 color    : SV_TARGET0;
};

PixelShaderOutput main(PixelShaderInput input) {
	PixelShaderOutput output;
	vec4 fragColor = vec4(0.0, 0.0, 0.0, 1);
	vec2 fragCoord = input.uv * iResolution.xy;
	vec3 color = 0.0;
	output.color = 0.0;

	// Early exit: avoid rendering outside the original viewport edges
	if (input.uv.x < x0 || input.uv.x > x1 ||
		input.uv.y < y0 || input.uv.y > y1)
	{
		output.color = 0.0;
		return output;
	}

	output.color = vec4(color, 1.0);
	return output;
}