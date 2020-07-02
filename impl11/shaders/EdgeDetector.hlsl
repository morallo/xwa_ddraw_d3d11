// Edge detectors from:
// https://www.shadertoy.com/view/4sf3W8
// Luminance formulas:
// https://stackoverflow.com/questions/596216/formula-to-determine-brightness-of-rgb-color
#include "ShaderToyDefs.h"
#include "shading_system.h"
#include "ShadertoyCBuffer.h"

#define thickness 0.0015

// The texture to process
Texture2D    procTex     : register(t0);
SamplerState procSampler : register(s0);

// The texture with the sub-component CMD
Texture2D    subCMDTex     : register(t1);
SamplerState subCMDSampler : register(s1);

static float4 LuminanceDot = float4(0.33, 0.5, 0.16, 0.15);

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

inline float sdCircle(in vec2 p, in vec2 center, float radius)
{
	return length(p - center) - radius;
}

PixelShaderOutput main(PixelShaderInput input) {
	PixelShaderOutput output;
	output.color = 0.0;

	// DEBUG
	//output.color = procTex.Sample(procSampler, input.uv);
	//return output;

	uint render2Denabled = SunCoords[3].w > 0.5;
	// p0, p1 hold the actual uv coords of the target box
	float2 uv = lerp(p0, p1, input.uv);
	float2 uvInGame = lerp(SunCoords[1].xy, SunCoords[1].zw, input.uv);
	// We expect iResolution and SunCoords to be 1.0 / ScreenResolution and 1.0 / InGameResolution respectively:
	float2 incr = iResolution.xy, incrInGame = SunCoords[2].xy;
	float2 start = -incr, startInGame = -incrInGame;

	float c[9];
	float4 col, subCMD = 0, subCMDtap;
	float2 ofs = start, ofsInGame;
	for (int i = 0; i < 3; ++i)
	{
		ofs.x = start.x;
		ofsInGame.x = startInGame.x;
		for (int j = 0; j < 3; ++j)
		{
			//float2 ofs = vec2(i - 1, j - 1) / iResolution.xy;
			//float2 ofsInGame = vec2(i - 1, j - 1) / inGameResolution.xy;
			col = procTex.SampleLevel(procSampler, uv + ofs, 0);
			// Approx Luminance formula:
			//c[3 * i + j] = 0.33 * col.r + 0.5 * col.g + 0.16 * col.b + 1.0 * col.a; // Add alpha here to make a hard edge around the objects
			c[3 * i + j] = dot(LuminanceDot, 4.0 * col);
			// Dilate the subCMD bracket:
			if (!render2Denabled) {
				subCMDtap = subCMDTex.SampleLevel(subCMDSampler, uvInGame + ofsInGame, 0);
				subCMD = max(subCMD, subCMDtap);
			}
			ofs.x += incr.x;
			ofsInGame.x += incr.x;
		}
		ofs.y += incr.y;
		ofsInGame.y += incr.y;
	}

	float Lx = 2.0 * (c[7] - c[1]) + c[6] + c[8] - c[2] - c[0];
	float Ly = 2.0 * (c[3] - c[5]) + c[6] + c[0] - c[2] - c[8];
	float G = sqrt(Lx*Lx + Ly*Ly);

	// SunColor[0] holds the tint to colorize the edge detector
	output.color = float4(G * SunColor[0].rgb, G);
	
	if (render2Denabled) {
		const float radius = iTime;
		const float d = sdCircle(uv, SunCoords[3].xy, radius * iResolution.y);
		subCMD = smoothstep(thickness, 0.0, abs(d)); // ring
	}

	// Add the sub-cmd:
	float alpha = max(subCMD.r, max(subCMD.g, subCMD.b));
	alpha > 0.1 ? 2.0 : 0.0;

	output.color.rgb = max(output.color.rgb, alpha);
	output.color.a = max(output.color.a, alpha);
	return output;
}