// Edge detectors from:
// https://www.shadertoy.com/view/4sf3W8
// Luminance formulas:
// https://stackoverflow.com/questions/596216/formula-to-determine-brightness-of-rgb-color
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

PixelShaderOutput main(PixelShaderInput input) {
	PixelShaderOutput output;
	output.color = 0.0;

	// DEBUG
	//output.color = procTex.Sample(procSampler, input.uv);
	//return output;

	// p0, p1 hold the actual uv coords of the target box
	float2 uv = lerp(p0, p1, input.uv);

	float c[9];
	float4 col;
	for (int i = 0; i < 3; ++i)
		for (int j = 0; j < 3; ++j) {
			vec2 ofs = vec2(i - 1, j - 1) / iResolution.xy;
			col = procTex.SampleLevel(procSampler, uv + ofs, 0);
			// Approx Luminance formula:
			c[3 * i + j] = 0.33 * col.r + 0.5 * col.g + 0.16 * col.b + 1.0 * col.a; // Add alpha here to make a hard edge around the objects
		}

	float Lx = 2.0 * (c[7] - c[1]) + c[6] + c[8] - c[2] - c[0];
	float Ly = 2.0 * (c[3] - c[5]) + c[6] + c[0] - c[2] - c[8];
	float G = sqrt(Lx*Lx + Ly*Ly);

	// SunColor[0] holds the tint to colorize the edge detector
	output.color = float4(G * SunColor[0].rgb, G);
	return output;
}