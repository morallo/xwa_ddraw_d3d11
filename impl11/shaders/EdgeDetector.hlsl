// Edge detectors from:
// https://www.shadertoy.com/view/4sf3W8
// https://www.shadertoy.com/view/XlSBzz
#include "ShaderToyDefs.h"
#include "shading_system.h"
#include "ShadertoyCBuffer.h"

// The texture to process
Texture2D    procTex     : register(t0);
SamplerState procSampler : register(s0);

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

/*
float lumi( in vec4 fragColor ) {
	return sqrt( 0.299*pow(fragColor.x, 2.0) + 0.587*pow(fragColor.y, 2.0) + 0.114*pow(fragColor.z, 2.0) );
}

vec2 sobel(in vec2 uv, in vec2 t) {
	float tleft = lumi(texture(iChannel0,uv + vec2(-t.x,t.y)));
	float left = lumi(texture(iChannel0,uv + vec2(-t.x,0)));
	float bleft = lumi(texture(iChannel0,uv + vec2(-t.x,-t.y)));
	float top = lumi(texture(iChannel0,uv + vec2(0,t.y)));
	float bottom = lumi(texture(iChannel0,uv + vec2(0,-t.y)));
	float tright = lumi(texture(iChannel0,uv + vec2(t.x,t.y)));
	float right = lumi(texture(iChannel0,uv + vec2(t.x,0)));
	float bright = lumi(texture(iChannel0,uv + vec2(t.x,-t.y)));
	float gx = tleft + 2.0*left + bleft - tright - 2.0*right - bright;
	float gy = -tleft - 2.0*top - tright + bleft + 2.0 * bottom + bright;
	return vec2(gx, gy);
}

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
	vec2 uv = fragCoord.xy / iResolution.xy;
	vec2 t = vec2(1.0) / iResolution.xy;
	vec2 s = sobel(uv, t);
	float g = sqrt(pow(s.x, 2.0) + pow(s.y, 2.0));
	vec3 col = vec3(g);
	fragColor = vec4(col, 1.0);
}
*/

PixelShaderOutput main(PixelShaderInput input) {
	PixelShaderOutput output;
	output.color = 0.0;

	// Early exit: avoid rendering outside the original viewport edges
	//if (any(input.uv < p0) || any(input.uv > p1))
	//	return output;

	// DEBUG
	//output.color = procTex.Sample(procSampler, input.uv);

	//vec3 c[9];
	float c[9];
	float3 col;
	for (int i = 0; i < 3; ++i)
		for (int j = 0; j < 3; ++j) {
			vec2 ofs = vec2(i - 1, j - 1) / iResolution.xy;
			col = procTex.Sample(procSampler, input.uv + ofs).rgb;
			c[3 * i + j] = dot(0.333, col);
		}

	float Lx = 2.0 * (c[7] - c[1]) + c[6] + c[8] - c[2] - c[0];
	float Ly = 2.0 * (c[3] - c[5]) + c[6] + c[0] - c[2] - c[8];
	float G = sqrt(Lx*Lx + Ly*Ly);

	output.color = float4(G,G,G, 1.0);
	return output;
}